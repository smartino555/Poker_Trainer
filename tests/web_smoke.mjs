// web_smoke.mjs - browser tests for the WebAssembly build (make test-web)
//
// Serves build/web with a tiny static server and drives it in headless Chromium:
// it loads, starts a game with the mouse, plays hands with the keyboard, fills the
// canvas at any size and pixel ratio, deals different cards on every visit, and shows
// an error instead of a stuck loading bar when a file is missing.
import { chromium } from 'playwright';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = process.env.WEB_DIR || path.join(path.dirname(fileURLToPath(import.meta.url)), '..', 'build', 'web');
const TYPES = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream' };
const VW = 1600, VH = 900;  // the game's virtual canvas

let pass = 0, fail = 0;
function check(ok, what) {
    if (ok) pass++; else { fail++; console.log('FAIL:', what); }
}

// ?missing=index.data makes the server 404 that file (for the loading-error test)
let missing = null;
const server = http.createServer((req, res) => {
    const name = decodeURIComponent(new URL(req.url, 'http://x').pathname).replace(/^\/+/, '') || 'index.html';
    const file = path.join(ROOT, path.basename(name));
    if (name === missing || !fs.existsSync(file)) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream' });
    fs.createReadStream(file).pipe(res);
});
await new Promise(r => server.listen(0, '127.0.0.1', r));
const URL_ = `http://127.0.0.1:${server.address().port}/index.html`;

const browser = await chromium.launch();

async function open(width, height, dpr = 1) {
    const page = await browser.newPage({ viewport: { width, height }, deviceScaleFactor: dpr });
    const errors = [];
    page.on('pageerror', e => errors.push(e.message));
    page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
    await page.goto(URL_);
    await page.waitForFunction(() => getComputedStyle(document.getElementById('status')).display === 'none', null, { timeout: 15000 });
    await page.waitForTimeout(500);
    return { page, errors };
}

// Click a point given in the game's virtual coordinates (the view is letterboxed to 16:9).
// The button is held for a few frames like a real click: the game polls once per frame.
async function click(page, vx, vy) {
    const { width, height } = page.viewportSize();
    const z = Math.min(width / VW, height / VH);
    const x = (width - VW * z) / 2 + vx * z, y = (height - VH * z) / 2 + vy * z;
    await page.mouse.move(x, y);
    await page.waitForTimeout(60);
    await page.mouse.down();
    await page.waitForTimeout(80);
    await page.mouse.up();
    await page.waitForTimeout(150);
}

const canvasPng = page => page.locator('#canvas').screenshot();

// 1. Loads, canvas covers the page at device-pixel resolution
for (const [w, h, dpr] of [[1600, 900, 1], [1280, 720, 2], [640, 360, 1], [900, 900, 1]]) {
    const { page, errors } = await open(w, h, dpr);
    const c = await page.evaluate(() => { const c = document.getElementById('canvas'); return [c.width, c.height, c.clientWidth, c.clientHeight]; });
    check(c[2] === w && c[3] === h, `${w}x${h}@${dpr}: canvas fills the page (css ${c[2]}x${c[3]})`);
    check(Math.abs(c[0] - w * dpr) <= 1 && Math.abs(c[1] - h * dpr) <= 1, `${w}x${h}@${dpr}: canvas is ${w * dpr}x${h * dpr} device pixels (got ${c[0]}x${c[1]})`);
    check(errors.length === 0, `${w}x${h}@${dpr}: no errors on load: ${errors.join(' | ')}`);
    await page.close();
}

// 2. Start from the menu with the mouse (Omaha, all Hard bots), then play hands with the keyboard
{
    const { page, errors } = await open(1280, 720, 2);
    const menu = await canvasPng(page);
    await click(page, 940, 260);   // Omaha
    await click(page, 1056, 486);  // set all bots to Hard
    await click(page, 555, 781);   // Start Game
    await page.waitForTimeout(1500);
    const table = await canvasPng(page);
    check(!menu.equals(table), 'mouse click on Start Game leaves the menu');
    const t0 = Date.now();
    let worst = 0;
    for (let i = 0; i < 40; i++) {
        await page.keyboard.press(i % 4 === 0 ? 'r' : 'c');
        await page.waitForTimeout(250);
        await page.keyboard.press('Space');
        // longest gap between two animation frames: a slow coach / bot decision would show here
        worst = Math.max(worst, await page.evaluate(() => new Promise(r => {
            let last = performance.now(), max = 0, n = 0;
            const f = t => { max = Math.max(max, t - last); last = t; if (++n < 10) requestAnimationFrame(f); else r(max); };
            requestAnimationFrame(f);
        })));
    }
    check(errors.length === 0, `40 hands of Omaha vs Hard bots without errors: ${errors.join(' | ')}`);
    check(worst < 1000, `no frame takes over 1 s during play (worst ${Math.round(worst)} ms)`);
    console.log(`  played for ${((Date.now() - t0) / 1000).toFixed(0)} s, worst frame gap ${Math.round(worst)} ms`);

    // Arrow keys and the wheel must not scroll an embedding page
    const prevented = await page.evaluate(() => {
        const e = new KeyboardEvent('keydown', { key: 'ArrowDown', cancelable: true, bubbles: true });
        document.getElementById('canvas').dispatchEvent(e);
        return e.defaultPrevented;
    });
    check(prevented, 'arrow keys do not scroll the host page');
    await page.close();
}

// 3. Keyboard text entry, including spaces, reaches the glossary search box
{
    const { page, errors } = await open(1600, 900, 1);
    await click(page, 1045, 781);   // menu: Glossary
    const empty = await canvasPng(page);
    await page.keyboard.type('pot odds');
    await page.waitForTimeout(300);
    const withSpace = await canvasPng(page);
    check(!empty.equals(withSpace), 'typing reaches the glossary search');
    for (let i = 0; i < 8; i++) await page.keyboard.press('Backspace');
    await page.keyboard.type('potodds');
    await page.waitForTimeout(300);
    const noSpace = await canvasPng(page);
    check(!withSpace.equals(noSpace), 'a typed space reaches the glossary search ("pot odds" differs from "potodds")');
    check(errors.length === 0, `glossary search without errors: ${errors.join(' | ')}`);
    await page.close();
}

// 4. Every visit deals different cards (seeded from more than the time since page load)
{
    const shots = [];
    for (let k = 0; k < 3; k++) {
        const { page } = await open(1600, 900, 1);
        await page.keyboard.press('Enter');   // Start Game from the menu
        await page.waitForTimeout(400);
        shots.push(await page.screenshot({ clip: { x: 480, y: 540, width: 240, height: 160 } }));  // your hole cards
        await page.close();
    }
    check(!(shots[0].equals(shots[1]) && shots[1].equals(shots[2])), 'three page loads do not all deal the same hand');
}

// 5. A missing file shows an error message instead of a stuck loading bar
for (const f of ['index.wasm', 'index.data']) {
    missing = f;
    const page = await browser.newPage();
    await page.goto(URL_);
    await page.waitForTimeout(3000);
    const msg = await page.evaluate(() => [getComputedStyle(document.getElementById('status')).display, document.getElementById('msg').textContent]);
    check(msg[0] !== 'none' && /could not start/.test(msg[1]), `missing ${f}: error shown (got "${msg[1]}")`);
    await page.close();
}
missing = null;

await browser.close();
server.close();
console.log(`\n${pass} checks, ${fail} failed`);
process.exit(fail ? 1 : 0);
