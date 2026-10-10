'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const { chromium } = require('playwright');

const source = fs.readFileSync('include/infrastructure/web/WebPage.h', 'utf8');
function embedded(tag) {
    const match = source.match(new RegExp('R"' + tag + '\\(([\\s\\S]*?)\\)' + tag + '";'));
    assert(match, `Missing embedded ${tag} resource`);
    return match[1];
}
const assets = {
    '/': ['text/html; charset=utf-8', embedded('html')],
    '/app.css': ['text/css; charset=utf-8', embedded('css')],
    '/app.js': ['application/javascript; charset=utf-8', embedded('js')],
};
const settings = {
    revision: 1, ssid: 'example-network', ip: '192.168.1.20', gateway: '192.168.1.1',
    subnet: '255.255.255.0', recipients: ['123'], message: 'Світло ввімкнене',
    startupSound: true, startupSoundSeconds: 10,
    wifiProgressSound: true, wifiConnectedSound: true,
    quietHoursEnabled: false, quietStartHour: 22, quietEndHour: 7,
    quietLedBrightnessPercent: 10,
    ledEnabled: true, deliveryBlink: true, startupLed: 1, connectingLed: 1,
    waitingLed: 0, idleLed: 1, errorLed: 1,
};
let testCalls = 0;
let signalSaves = 0;
let wifiRequests = 0;
let previewRequests = 0;
const previewLevels = [];
const server = http.createServer(async (request, response) => {
    const path = new URL(request.url, 'http://localhost').pathname;
    if (assets[path]) {
        const [type, body] = assets[path];
        response.writeHead(200, { 'Content-Type': type });
        response.end(body);
        return;
    }
    const parts = [];
    for await (const chunk of request) parts.push(chunk);
    const form = new URLSearchParams(Buffer.concat(parts).toString('utf8'));
    const send = (code, data, headers = {}) => {
        response.writeHead(code, { 'Content-Type': 'application/json', ...headers });
        response.end(JSON.stringify(data));
    };
    if (path === '/api/login') {
        send(200, { csrf: 'test-csrf' }, { 'Set-Cookie': 'lon_session=test; HttpOnly; SameSite=Strict' });
        return;
    }
    if (!request.headers.cookie?.includes('lon_session=test')) {
        send(401, { error: 'Authentication required' });
        return;
    }
    if (request.method === 'POST') {
        assert.equal(request.headers['x-csrf'], 'test-csrf');
    }
    if (path === '/api/session') send(200, { csrf: 'test-csrf' });
    else if (path === '/api/settings') send(200, settings);
    else if (path === '/api/status') send(200, {
        wifi: 'connected', ip: settings.ip, uptimeMs: 60000, firmware: 'test', timeReady: true,
        delivery: 'delivered', wifiPending: false, recovery: false, storageReady: true,
        ledActive: true, buzzerActive: false, recipients: [{ index: 0, outcome: 'delivered', attempts: 1 }],
        quietHoursEnabled: settings.quietHoursEnabled, quietHoursActive: settings.quietHoursEnabled,
        tests: [0], testActive: false,
    });
    else if (path === '/api/signals') {
        signalSaves += 1;
        assert.equal(form.get('startupSound'), '0');
        assert.equal(form.get('startupSoundSeconds'), '3');
        assert.equal(form.get('quietHoursEnabled'), '1');
        assert.equal(form.get('quietStartHour'), '23');
        assert.equal(form.get('quietEndHour'), '6');
        assert.equal(form.get('quietLedBrightnessPercent'), '17');
        settings.startupSound = false;
        settings.startupSoundSeconds = 3;
        settings.quietHoursEnabled = true;
        settings.quietStartHour = 23;
        settings.quietEndHour = 6;
        settings.quietLedBrightnessPercent = 17;
        settings.revision += 1;
        send(200, { saved: true });
    } else if (path === '/api/test') {
        testCalls += 1;
        assert.equal(form.get('mask'), '1');
        send(202, { queued: true });
    } else if (path === '/api/wifi') {
        wifiRequests += 1;
        response.writeHead(202, { 'Content-Type': 'application/json' });
        response.end();
    } else if (path === '/api/preview') {
        previewRequests += 1;
        assert.equal(form.get('kind'), 'led');
        assert.equal(form.get('mode'), '1');
        previewLevels.push(form.get('brightnessPercent'));
        if (previewRequests === 1) {
            response.writeHead(413, { 'Content-Length': '0' });
            response.end();
        } else send(202, { preview: true });
    }
    else send(404, { error: 'Not found' });
});

(async () => {
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const port = server.address().port;
    const browser = await chromium.launch({
        channel: process.env.PW_CHANNEL || 'chromium', headless: true,
    });
    try {
        const page = await browser.newPage({ viewport: { width: 360, height: 780 } });
        const external = [];
        page.on('request', req => {
            if (!req.url().startsWith(`http://127.0.0.1:${port}/`)) external.push(req.url());
        });
        await page.goto(`http://127.0.0.1:${port}/`);
        await page.getByLabel('Пароль', { exact: true }).fill('device-secret');
        await page.getByRole('button', { name: 'Увійти' }).click();
        await page.getByRole('heading', { name: 'Стан пристрою' }).waitFor();
        assert.equal(testCalls, 0);
        assert.equal(external.length, 0);
        await page.getByRole('button', { name: 'Звук і світлодіод' }).click();
        assert.deepEqual(await page.locator('#previewMode option').allTextContents(),
            ['Вимкнено', 'Світиться постійно', 'Блимає']);
        await page.getByLabel('Звук при появі світла').uncheck();
        await page.getByLabel('Тривалість звуку при появі світла (секунди)').fill('3');
        await page.getByLabel('Нічний режим: тиша зумера та нічна яскравість LED').check();
        await page.getByLabel('Початок тиші (година, 0–23)').fill('23');
        await page.getByLabel('Кінець тиші (година, 0–23)').fill('6');
        const brightness = page.getByLabel('Нічна яскравість світлодіода (%)');
        assert.equal(await brightness.inputValue(), '10');
        await brightness.fill('101');
        await page.getByRole('button', { name: 'Зберегти' }).click();
        assert.equal(signalSaves, 0);
        assert.equal(await brightness.evaluate(node => node.checkValidity()), false);
        await brightness.fill('-1');
        assert.equal(await brightness.evaluate(node => node.checkValidity()), false);
        await brightness.fill('0');
        assert.equal(await brightness.evaluate(node => node.checkValidity()), true);
        await brightness.fill('100');
        assert.equal(await brightness.evaluate(node => node.checkValidity()), true);
        await brightness.fill('17');
        await page.getByRole('button', { name: 'Зберегти' }).click();
        await page.getByText('Збережено', { exact: true }).waitFor();
        assert.equal(signalSaves, 1);
        assert.equal(await page.getByLabel('Тривалість звуку при появі світла (секунди)').inputValue(), '3');
        assert.equal(await page.getByLabel('Нічний режим: тиша зумера та нічна яскравість LED').isChecked(), true);
        assert.equal(await page.getByLabel('Початок тиші (година, 0–23)').inputValue(), '23');
        assert.equal(await page.getByLabel('Кінець тиші (година, 0–23)').inputValue(), '6');
        assert.equal(await brightness.inputValue(), '17');
        await page.reload();
        await page.getByRole('heading', { name: 'Стан пристрою' }).waitFor();
        await page.getByRole('button', { name: 'Звук і світлодіод' }).click();
        assert.equal(await brightness.inputValue(), '17');
        assert.equal(testCalls, 0);
        const previewBrightness = page.getByLabel('Яскравість світлодіода для перегляду (%)');
        assert.equal(await previewBrightness.inputValue(), '100');
        await page.getByLabel('Режим світлодіода для перегляду').selectOption('1');
        for (const value of ['101', '-1', '1.5', '']) {
            await previewBrightness.fill(value);
            await page.getByRole('button', { name: 'Світлодіод', exact: true }).click();
            assert.equal(previewRequests, 0);
        }
        await previewBrightness.fill('37');
        await page.getByRole('button', { name: 'Світлодіод', exact: true }).click();
        await page.getByText('HTTP-заголовки або дані запиту завеликі').waitFor();
        await page.getByRole('button', { name: 'Світлодіод', exact: true }).click();
        await page.getByText('Попередній перегляд запущено').waitFor();
        assert.equal(previewRequests, 2);
        for (const value of ['0', '100']) {
            await previewBrightness.fill(value);
            await Promise.all([
                page.waitForResponse(response => response.url().endsWith('/api/preview')),
                page.getByRole('button', { name: 'Світлодіод', exact: true }).click(),
            ]);
        }
        assert.deepEqual(previewLevels, ['37', '37', '0', '100']);
        assert.equal(signalSaves, 1);
        assert.equal(settings.quietLedBrightnessPercent, 17);
        assert.equal(await page.locator('body').evaluate(node => node.scrollWidth <= innerWidth), true);
        await page.getByRole('button', { name: 'Стан', exact: true }).click();
        page.once('dialog', dialog => dialog.accept());
        await page.getByRole('button', { name: 'Надіслати тестове повідомлення' }).click();
        assert.equal(testCalls, 1);
        await page.getByRole('button', { name: 'Wi-Fi', exact: true }).click();
        page.once('dialog', dialog => dialog.accept());
        await page.getByRole('button', { name: 'Застосувати та перевірити' }).click();
        await page.getByText('Відповідь не підтверджена.', { exact: false }).waitFor();
        assert.equal(wifiRequests, 1);
        assert.equal(await page.getByText('Відкрити 192.168.1.20').getAttribute('href'),
            'http://192.168.1.20/');
        console.log('Browser checks passed: login, mobile layout, signal save, explicit test send, interrupted Wi-Fi response.');
    } finally {
        await browser.close();
        server.close();
    }
})().catch(error => {
    console.error(error);
    server.close();
    process.exitCode = 1;
});
