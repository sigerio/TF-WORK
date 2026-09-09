"""Generate an offline preview from the real C effect library (requires host cc)."""
from pathlib import Path
import json
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="xj_ws2812_preview_") as directory:
    binary = str(Path(directory) / "effects")
    command = ["cc", "-std=c99", "-DGD32F10X_HD", "-ICMSIS", "-ICore", "-IGD32F10X/Include", "-IUSER",
               "-Wno-pointer-to-int-cast", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
               "tests/test_ws2812_debug.c", "USER/CONTROL/ws2812/ws2812_status.c", "USER/CONTROL/ws2812/ws2812_effect.c",
               "USER/COMMON/ringbuf.c", "-o", binary]
    subprocess.run(command, cwd=ROOT, check=True)
    frames = json.loads(subprocess.check_output([binary, "--preview"], cwd=ROOT, text=True))

html = '''<!doctype html>
<html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>机库灯效 · 机械科幻</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#080e14;color:#d9e9f2}
body{max-width:1100px;margin:40px auto;padding:0 24px}h1{font-size:30px;letter-spacing:3px;margin:8px 0 16px}
p{color:#97adbc;line-height:1.7}.tag{font:12px monospace;letter-spacing:3px;color:#72b7c6}
.controls{display:flex;gap:18px;align-items:center;flex-wrap:wrap;margin:28px 0}
button,select{background:#172733;color:#e2f3fc;border:1px solid #345365;border-radius:5px;padding:10px 14px;cursor:pointer}
input{flex:1;min-width:150px;accent-color:#78bfd6}output{font:14px monospace;min-width:55px}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}
article{border:1px solid #273744;border-radius:8px;padding:22px;background:linear-gradient(150deg,#13212b,#0b141c)}
h2{font-size:19px;font-weight:500;margin:0 0 8px}code{font:12px monospace;color:#87c8d8;margin-right:12px}
article p{font-size:13px;min-height:42px;margin:12px 0 0}.strip{display:flex;justify-content:space-between;background:#05090d;border-radius:7px;padding:30px 12px;margin:22px 0}
.led{width:16px;height:22px;border-radius:3px;background:#000;border:1px solid #25303a}
@media(max-width:720px){.grid{grid-template-columns:1fr}body{padding:0 16px}.led{width:5%;max-width:22px}}
</style>
<div class="tag">HANGAR / LIGHTING STUDY</div><h1>机械科幻 · 六种灯效</h1>
<p>冷白与冰蓝构成主体，琥珀色强调机械运动。保留蓄能、停顿和余辉，使用现有 12 颗灯珠。</p>
<div class="controls"><button id="play" type="button">暂停</button><button id="restart" type="button">从头播放</button>
<label>速度 <select id="speed"><option value="0.5">0.5×</option><option value="1" selected>1×</option><option value="2">2×</option></select></label>
<input id="time" type="range" min="0" max="479" value="0" aria-label="播放时间"><output id="stamp">0.00 s</output></div>
<main class="grid"></main>
<p>预览使用固件 C 灯效库直接生成的 RGB 帧，每 50 ms 一帧，共 24 秒，片段结束后从头播放。
屏幕上的光晕为示意，灯带的实际亮度和颜色以烧录后的实物为准。</p>
<script>
const frames = __FRAMES__;
const names = ['反应堆心跳','双转子涡轮','重载扫描','冲击涟漪','低温等离子','跃迁蓄能'];
const descriptions = ['一强一弱双脉冲，核心向两端扩散，随后回到深蓝待机。3.6 秒循环。',
'冰蓝与琥珀拖尾反向旋转，交会时光强叠加。每圈 3.2 秒。',
'冷白扫描核与琥珀光晕缓起缓停，两端各停留 600 ms。4.8 秒循环。',
'中央点火，冷白冲击波向两端扩散，青色余波随后消退。2.8 秒循环。',
'蓝青两层光带以不同速度缓慢交叠，形成低温等离子流。',
'两端汇聚蓄能，锁定后短暂冷白释放，再逐渐冷却。6 秒循环。'];
const grid = document.querySelector('.grid');
const leds = names.map((name, index) => {
 const card = document.createElement('article');
 card.innerHTML = `<h2><code>WSE:${index + 1}</code>${name}</h2><div class="strip" role="img" aria-label="${name}灯带动画"></div><p>${descriptions[index]}</p>`;
 grid.append(card);
 return Array.from({length:frames[index][0].length}, () => {
  const led = document.createElement('span');led.className='led';card.querySelector('.strip').append(led);return led;
 });
});
const play = document.getElementById('play'), slider = document.getElementById('time'), speed = document.getElementById('speed');
let running = !matchMedia('(prefers-reduced-motion: reduce)').matches, time = 0, last = performance.now(), shown = -1;
play.textContent = running ? '暂停' : '播放';
play.onclick = () => {running = !running;play.textContent = running ? '暂停' : '播放';};
document.getElementById('restart').onclick = () => {time = 0;shown = -1;};
slider.oninput = () => {time = Number(slider.value)*50;shown = -1;};
function animate(now) {
 if (running && !document.hidden) time = (time + Math.min(now-last,100)*Number(speed.value)) % 24000;
 last = now;
 const frame = Math.floor(time/50);
 if (frame !== shown) {
  shown = frame;slider.value = frame;document.getElementById('stamp').textContent = (frame/20).toFixed(2)+' s';
  leds.forEach((strip, effect) => strip.forEach((led, index) => {
   const value = frames[effect][frame][index], rgb = `${value>>>16},${(value>>>8)&255},${value&255}`;
   led.style.background = `rgb(${rgb})`;led.style.boxShadow = `0 0 12px rgba(${rgb},.65),0 0 28px rgba(${rgb},.2)`;
  }));
 }
 requestAnimationFrame(animate);
}
requestAnimationFrame(animate);
</script></html>
'''
output = ROOT / "docs/ws2812_preview.html"
output.write_text(html.replace("__FRAMES__", json.dumps(frames, separators=(",", ":"))))
print(output)
