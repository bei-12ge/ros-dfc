#include "ros/report.h"

#include "ros/timestamp.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

namespace {

/// JSON / JS 字符串转义：同时把 < > 转成 \\u 形式，避免注入 script 标签
std::string jsonEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const char ch : in) {
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            case '<':
                out += "\\u003c";
                break;
            case '>':
                out += "\\u003e";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out += ' ';
                } else {
                    out += ch;
                }
                break;
        }
    }
    return out;
}

/// 插入 HTML 文本节点用的最小转义
std::string htmlEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const char ch : in) {
        switch (ch) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            default:
                out += ch;
                break;
        }
    }
    return out;
}

/// 判断一段文本是否形如 NODE-001 / SERVICE-006
bool isErrorCode(const std::string& text) {
    if (text.size() < 5) {
        return false;
    }
    bool seenDash = false;
    bool seenDigit = false;
    for (const char ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            continue;
        }
        if (ch == '-') {
            if (seenDash) {
                return false;
            }
            seenDash = true;
            continue;
        }
        if (ch >= '0' && ch <= '9') {
            seenDigit = true;
            continue;
        }
        return false;
    }
    return seenDash && seenDigit;
}

const char* const kHtmlTemplate = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E%3Crect width='32' height='32' fill='%23151c22'/%3E%3Ccircle cx='8' cy='16' r='3.5' fill='%237fb2c8'/%3E%3Ccircle cx='24' cy='8' r='3.5' fill='%23d9a441'/%3E%3Ccircle cx='24' cy='24' r='3.5' fill='%238fb37a'/%3E%3Cpath d='M11 15 L21 9 M11 17 L21 23' stroke='%23364a56' stroke-width='1.5' fill='none'/%3E%3C/svg%3E">
<style>
:root{
  --bg:#0f1419; --panel:#151c22; --panel-2:#1a222a; --line:#243440;
  --text:#c9d3db; --dim:#7d8d99; --info:#7fb2c8; --warn:#d9a441;
  --err:#d4695f; --ok:#8fb37a; --mono:"Cascadia Mono","Consolas","SFMono-Regular",ui-monospace,monospace;
}
*{box-sizing:border-box}
html,body{margin:0}
body{background:var(--bg);color:var(--text);font-family:"Segoe UI","Microsoft YaHei",system-ui,sans-serif;font-size:14px;line-height:1.55}
.wrap{max-width:1600px;margin:0 auto;padding:28px 30px 44px}
header{display:flex;flex-wrap:wrap;align-items:flex-end;justify-content:space-between;gap:16px;padding-bottom:16px;border-bottom:1px solid var(--line)}
h1{margin:0;font-size:23px;font-weight:600;letter-spacing:.02em}
.sub{margin:7px 0 0;color:var(--dim);font-size:12.5px}
.stamp{color:var(--dim);font-size:12px;text-align:right;font-family:var(--mono)}
.kpis{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:1px;background:var(--line);border:1px solid var(--line);margin-top:20px}
.kpi{background:var(--panel);padding:14px 16px}
.kpi .n{font-family:var(--mono);font-size:24px;line-height:1.1;color:#e5edf3}
.kpi .l{margin-top:5px;font-size:12px;color:var(--dim)}
.kpi.err .n{color:var(--err)}
.kpi.warn .n{color:var(--warn)}
.main{display:grid;grid-template-columns:minmax(0,1fr) 380px;gap:18px;margin-top:18px;align-items:start}
.panel{background:var(--panel);border:1px solid var(--line)}
.panel h2{margin:0;padding:12px 16px;font-size:13.5px;font-weight:600;border-bottom:1px solid var(--line);display:flex;justify-content:space-between;align-items:center;gap:10px}
.panel h2 span{font-family:var(--mono);font-size:11.5px;color:var(--dim);font-weight:400}
.bar{display:flex;flex-wrap:wrap;gap:8px;align-items:center;padding:10px 16px;border-bottom:1px solid var(--line)}
.chip{font-family:var(--mono);font-size:11.5px;padding:3px 9px;border:1px solid var(--line);color:var(--dim);background:transparent;cursor:pointer;border-radius:2px}
.chip[aria-pressed="true"]{color:#e5edf3;border-color:#3d5464;background:var(--panel-2)}
.chip[data-level="ERROR"][aria-pressed="true"]{color:var(--err);border-color:#4b2b28}
.chip[data-level="WARN"][aria-pressed="true"]{color:var(--warn);border-color:#4a3a1e}
.chip[data-level="INFO"][aria-pressed="true"]{color:var(--info);border-color:#26404d}
.search{flex:1;min-width:150px;background:var(--panel-2);border:1px solid var(--line);color:var(--text);padding:5px 9px;font-size:12.5px;border-radius:2px}
.search:focus{outline:1px solid #3d5464;outline-offset:0}
.events{max-height:660px;overflow:auto}
table{border-collapse:collapse;width:100%;table-layout:fixed}
th,td{text-align:left;vertical-align:top;padding:6px 10px;font-size:12.5px;border-bottom:1px solid #1d2831}
th{position:sticky;top:0;background:var(--panel-2);color:var(--dim);font-weight:500;font-size:12px;z-index:2}
td{font-family:var(--mono);color:#b7c3cc;word-break:break-word;white-space:pre-wrap}
td.t{color:#6f8290;white-space:nowrap}
td.lv{font-size:11.5px}
td.lv.INFO{color:var(--info)} td.lv.WARN{color:var(--warn)} td.lv.ERROR{color:var(--err)}
td.cd{color:#9db3a6;font-size:11.5px}
td.mod{color:#8b9db0;font-size:11.5px}
.side{display:flex;flex-direction:column;gap:18px}
.topic{display:block;padding:12px 14px;border-bottom:1px solid #1d2831}
.topic:last-child{border-bottom:0}
.thead{display:flex;align-items:baseline;justify-content:space-between;gap:10px;margin-bottom:9px}
.tcols{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:12px}
.tcol{min-width:0}
.cap{font-size:11px;color:#63767f;margin-bottom:5px}
.nodes{display:flex;flex-wrap:wrap;gap:5px}
.n{border:1px solid #2d404d;padding:2px 7px;font-family:var(--mono);font-size:11.5px;color:#aebecb;white-space:nowrap}
.n.pub{border-color:#26414f;color:#9fc4d4}
.n.sub{border-color:#3a4a33;color:#a9c69a}
.n.dim{border-color:#22303a;color:#6f8290}
.pill{font-family:var(--mono);font-size:11.5px;padding:3px 9px;background:#1d2932;border:1px solid #33474f;color:#cfe0e8;white-space:nowrap}
.stats{font-size:11px;color:#6f8290;white-space:nowrap}
.empty{padding:16px;color:var(--dim);font-size:12.5px}
.mods{padding:12px 16px 14px;display:flex;flex-direction:column;gap:9px}
.modrow{display:grid;grid-template-columns:96px minmax(0,1fr) 46px;gap:10px;align-items:center;font-size:12px}
.modrow .mname{font-family:var(--mono);color:#93a6b5;font-size:11.5px}
.track{height:8px;background:#1b262d;position:relative}
.fill{position:absolute;left:0;top:0;bottom:0;background:#3f6b7d}
.modrow .cnt{font-family:var(--mono);font-size:11.5px;color:var(--dim);text-align:right}
.exc{margin-top:18px}
.tag{font-family:var(--mono);font-size:11px;padding:2px 7px;border:1px solid #4b2b28;color:var(--err);white-space:nowrap}
.svc{margin-top:18px}
.svc .topic .pill{border-color:#4b3d24;color:#e2c99a}
footer{margin-top:22px;padding-top:14px;border-top:1px solid var(--line);color:var(--dim);font-size:11.5px;display:flex;flex-wrap:wrap;gap:14px;justify-content:space-between}
footer .mono{font-family:var(--mono)}
@media (max-width:1080px){
  .main{grid-template-columns:minmax(0,1fr)}
  .kpis{grid-template-columns:repeat(2,minmax(0,1fr))}
  .tcols{grid-template-columns:minmax(0,1fr)}
}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <div>
      <h1>__TITLE__</h1>
      <p class="sub">本页由 ros_demo 运行时实时采集的日志渲染而成，不依赖任何外部资源；数据随每次运行重新生成。</p>
    </div>
    <div class="stamp">生成时间 __GENERATED__<br>日志事件 __COUNT__ 条</div>
  </header>

  <div class="kpis">
    <div class="kpi"><div class="n" id="k-event">0</div><div class="l">日志事件总数</div></div>
    <div class="kpi"><div class="n" id="k-node">0</div><div class="l">参与节点（发布/订阅/服务）</div></div>
    <div class="kpi warn"><div class="n" id="k-topic">0</div><div class="l">话题与服务通道</div></div>
    <div class="kpi err"><div class="n" id="k-err">0</div><div class="l">被拦截的异常</div></div>
  </div>

  <div class="main">
    <div class="panel">
      <h2>事件流 <span id="ev-count">—</span></h2>
      <div class="bar">
        <button class="chip" data-level="all" aria-pressed="true">全部</button>
        <button class="chip" data-level="INFO" aria-pressed="false">INFO</button>
        <button class="chip" data-level="WARN" aria-pressed="false">WARN</button>
        <button class="chip" data-level="ERROR" aria-pressed="false">ERROR</button>
        <input class="search" id="q" type="search" placeholder="搜索节点名、话题、错误码…">
      </div>
      <div class="events">
        <table>
          <colgroup>
            <col style="width:96px"><col style="width:62px"><col style="width:88px"><col style="width:92px"><col>
          </colgroup>
          <thead><tr><th>时间</th><th>级别</th><th>模块</th><th>错误码</th><th>内容</th></tr></thead>
          <tbody id="rows"></tbody>
        </table>
      </div>
    </div>

    <div class="side">
      <div class="panel">
        <h2>话题拓扑 <span>发布 → 话题 → 订阅</span></h2>
        <div id="topo"></div>
      </div>
      <div class="panel">
        <h2>模块事件分布 <span>按日志来源统计</span></h2>
        <div class="mods" id="mods"></div>
      </div>
    </div>
  </div>

  <div class="panel svc">
    <h2>服务通道 <span>客户端 → 服务端 同步一问一答</span></h2>
    <div id="svc"></div>
  </div>

  <div class="panel exc">
    <h2>异常拦截清单 <span id="exc-count">—</span></h2>
    <div class="events">
      <table>
        <colgroup><col style="width:96px"><col style="width:74px"><col style="width:122px"><col></colgroup>
        <thead><tr><th>时间</th><th>模块</th><th>错误码</th><th>被拦截的异常</th></tr></thead>
        <tbody id="exc"></tbody>
      </table>
    </div>
  </div>

  <footer>
    <span>数据来源：ros_sim::Logger 运行时日志捕获（内存缓冲）</span>
    <span class="mono">ros_demo · C++ 模拟 ROS 节点系统</span>
  </footer>
</div>

<script>
const DATA = __DATA__;

const byLevel = {INFO:0, WARN:0, ERROR:0};
DATA.forEach(e => { byLevel[e.l] = (byLevel[e.l] || 0) + 1; });

// ---------- 从日志文本推导拓扑 ----------
const nodeOf = s => { const m = /节点=([^\s,，]+)/.exec(s); return m ? m[1] : ''; };
const topicOf = s => { const m = /话题=([^\s,，]+)/.exec(s); return m ? m[1] : ''; };
const svcOf = s => { const m = /服务=([^\s,，]+)/.exec(s); return m ? m[1] : ''; };

const pubs = new Map();   // topic -> Set(node)
const subs = new Map();   // topic -> Set(node)
const srv = new Map();    // service -> Set(node)
const cli = new Map();    // service -> Set(node)
const push = (map, key, val) => {
  if (!key || !val) return;
  if (!map.has(key)) map.set(key, new Set());
  map.get(key).add(val);
};

const pubCount = new Map();
const subCount = new Map();
DATA.forEach(e => {
  const msg = e.x;
  if (msg.startsWith('注册发布者')) push(pubs, topicOf(msg), nodeOf(msg));
  else if (msg.startsWith('注册订阅者')) push(subs, topicOf(msg), nodeOf(msg));
  else if (msg.startsWith('注册服务端')) push(srv, svcOf(msg), nodeOf(msg));
  else if (msg.startsWith('客户端节点初始化完成')) push(cli, svcOf(msg), nodeOf(msg));
  else if (e.m === 'PUB' && msg.startsWith('发布消息')) {
    const t = topicOf(msg); pubCount.set(t, (pubCount.get(t) || 0) + 1);
  } else if (e.m === 'SUB' && msg.startsWith('[接收]')) {
    const t = topicOf(msg); subCount.set(t, (subCount.get(t) || 0) + 1);
  }
});

const allNodes = new Set();
[pubs, subs, srv, cli].forEach(map => map.forEach(set => set.forEach(v => allNodes.add(v))));
const topics = new Set([...pubs.keys(), ...subs.keys()]);

document.getElementById('k-event').textContent = DATA.length;
document.getElementById('k-node').textContent = allNodes.size;
document.getElementById('k-topic').textContent = topics.size + srv.size;
document.getElementById('k-err').textContent = byLevel.ERROR || 0;

// ---------- 话题拓扑 ----------
(function renderTopo() {
  const host = document.getElementById('topo');
  if (!topics.size) { host.innerHTML = '<div class="empty">本次运行未注册话题</div>'; return; }
  const names = [...topics].sort();
  host.innerHTML = names.map(t => {
    const p = [...(pubs.get(t) || new Set())].sort();
    const s = [...(subs.get(t) || new Set())].sort();
    const left = p.length ? p.map(n => `<span class="n pub">${n}</span>`).join('') : '<span class="n dim">无发布者</span>';
    const right = s.length ? s.map(n => `<span class="n sub">${n}</span>`).join('') : '<span class="n dim">无订阅者</span>';
    return `<div class="topic">
      <div class="thead">
        <span class="pill">${t}</span>
        <span class="stats">发布 ${pubCount.get(t) || 0} · 接收 ${subCount.get(t) || 0}</span>
      </div>
      <div class="tcols">
        <div class="tcol"><div class="cap">发布者</div><div class="nodes">${left}</div></div>
        <div class="tcol"><div class="cap">订阅者</div><div class="nodes">${right}</div></div>
      </div>
    </div>`;
  }).join('');
})();

// ---------- 服务通道 ----------
(function renderSvc() {
  const host = document.getElementById('svc');
  const services = new Set([...srv.keys(), ...cli.keys()]);
  if (!services.size) { host.innerHTML = '<div class="empty">本次运行未注册服务</div>'; return; }
  host.innerHTML = [...services].sort().map(s => {
    const c = [...(cli.get(s) || new Set())].sort();
    const v = [...(srv.get(s) || new Set())].sort();
    const left = c.length ? c.map(n => `<span class="n pub">${n}</span>`).join('') : '<span class="n dim">无客户端</span>';
    const right = v.length ? v.map(n => `<span class="n sub">${n}</span>`).join('') : '<span class="n dim">无服务端</span>';
    const ghost = !v.length;
    return `<div class="topic">
      <div class="thead">
        <span class="pill">${s}</span>
        <span class="stats">${ghost ? '服务端缺失，调用被拦截' : '服务端已注册'}</span>
      </div>
      <div class="tcols">
        <div class="tcol"><div class="cap">客户端</div><div class="nodes">${left}</div></div>
        <div class="tcol"><div class="cap">服务端</div><div class="nodes">${right}</div></div>
      </div>
    </div>`;
  }).join('');
})();

// ---------- 模块分布 ----------
(function renderMods() {
  const counts = new Map();
  DATA.forEach(e => counts.set(e.m, (counts.get(e.m) || 0) + 1));
  const list = [...counts.entries()].sort((a, b) => b[1] - a[1]).slice(0, 8);
  const max = list.length ? list[0][1] : 1;
  document.getElementById('mods').innerHTML = list.map(([m, n]) =>
    `<div class="modrow"><span class="mname">${m}</span><div class="track"><div class="fill" style="width:${(n / max * 100).toFixed(1)}%"></div></div><span class="cnt">${n}</span></div>`
  ).join('');
})();

// ---------- 事件流 ----------
let level = 'all';
let query = '';
const rowsHost = document.getElementById('rows');
const evCount = document.getElementById('ev-count');

function renderRows() {
  const q = query.trim().toLowerCase();
  const list = DATA.filter(e =>
    (level === 'all' || e.l === level) &&
    (!q || (e.x + ' ' + e.m + ' ' + e.c).toLowerCase().includes(q)));
  evCount.textContent = `显示 ${list.length} / ${DATA.length} 条`;
  rowsHost.innerHTML = list.map(e =>
    `<tr><td class="t">${e.t.slice(11)}</td><td class="lv ${e.l}">${e.l}</td><td class="mod">${e.m}</td><td class="cd">${e.c}</td><td>${e.x}</td></tr>`
  ).join('');
}
renderRows();

document.querySelectorAll('.chip').forEach(chip => {
  chip.addEventListener('click', () => {
    level = chip.dataset.level;
    document.querySelectorAll('.chip').forEach(c => c.setAttribute('aria-pressed', String(c === chip)));
    renderRows();
  });
});
document.getElementById('q').addEventListener('input', ev => {
  query = ev.target.value;
  renderRows();
});

// ---------- 异常清单 ----------
(function renderExc() {
  const errors = DATA.filter(e => e.l === 'ERROR');
  document.getElementById('exc-count').textContent = errors.length ? `${errors.length} 项` : '无';
  document.getElementById('exc').innerHTML = errors.length
    ? errors.map(e => `<tr><td class="t">${e.t.slice(11)}</td><td class="mod">${e.m}</td><td class="cd"><span class="tag">${e.c}</span></td><td>${e.x}</td></tr>`).join('')
    : '<tr><td colspan="4" class="empty">本次运行没有触发异常拦截</td></tr>';
})();
</script>
</body>
</html>
)HTML";

}  // namespace

namespace ros_sim {

namespace report {

bool parseLine(const std::string& raw, LogLine& out) {
    out = LogLine{};
    if (raw.size() < 4 || raw.front() != '[') {
        return false;
    }
    const size_t endTime = raw.find(']');
    if (endTime == std::string::npos) {
        return false;
    }
    out.time = raw.substr(1, endTime - 1);

    size_t levelOpen = raw.find('[', endTime + 1);
    if (levelOpen == std::string::npos) {
        return false;
    }
    const size_t levelClose = raw.find(']', levelOpen + 1);
    if (levelClose == std::string::npos) {
        return false;
    }
    out.level = raw.substr(levelOpen + 1, levelClose - levelOpen - 1);

    const size_t modOpen = raw.find('[', levelClose + 1);
    if (modOpen == std::string::npos) {
        return false;
    }
    const size_t modClose = raw.find(']', modOpen + 1);
    if (modClose == std::string::npos) {
        return false;
    }
    out.module = raw.substr(modOpen + 1, modClose - modOpen - 1);

    size_t pos = modClose + 1;
    if (pos < raw.size() && raw[pos] == ' ') {
        ++pos;
    }
    std::string rest = raw.substr(pos);
    // 异常行形如 "[NODE-001] 节点名称不能为空"，单独抽出错误码便于统计与筛选。
    // 只认「大写字母 + 连字符 + 数字」的形状，避免把 "[接收]" 这类业务标记误判成错误码。
    if (!rest.empty() && rest.front() == '[') {
        const size_t codeClose = rest.find(']');
        if (codeClose != std::string::npos && codeClose > 1 && isErrorCode(rest.substr(1, codeClose - 1))) {
            out.code = rest.substr(1, codeClose - 1);
            size_t next = codeClose + 1;
            if (next < rest.size() && rest[next] == ' ') {
                ++next;
            }
            rest = rest.substr(next);
        }
    }
    out.message = rest;
    return true;
}

bool writeHtml(const std::string& htmlPath,
               const std::vector<std::string>& rawLines,
               const std::string& title) {
    std::string data;
    data.reserve(rawLines.size() * 160);
    data.push_back('[');
    size_t emitted = 0;
    for (const std::string& raw : rawLines) {
        LogLine line;
        if (!parseLine(raw, line)) {
            continue;
        }
        if (emitted++ > 0) {
            data.push_back(',');
        }
        data += "{\"t\":\"" + jsonEscape(line.time);
        data += "\",\"l\":\"" + jsonEscape(line.level);
        data += "\",\"m\":\"" + jsonEscape(line.module);
        data += "\",\"c\":\"" + jsonEscape(line.code);
        data += "\",\"x\":\"" + jsonEscape(line.message) + "\"}";
    }
    data.push_back(']');

    std::string html = kHtmlTemplate;

    // 模板里同一个占位符可能出现多次（例如标题同时出现在 <title> 与 <h1>），必须全部替换
    auto replaceAll = [](std::string& text, const std::string& from, const std::string& to) {
        size_t pos = text.find(from);
        while (pos != std::string::npos) {
            text.replace(pos, from.size(), to);
            pos = text.find(from, pos + to.size());
        }
    };
    const std::string placeholderTitle = "__TITLE__";
    const std::string placeholderData = "__DATA__";
    const std::string placeholderCount = "__COUNT__";
    const std::string placeholderGen = "__GENERATED__";

    if (html.find(placeholderData) == std::string::npos) {
        return false;
    }
    replaceAll(html, placeholderTitle, htmlEscape(title));
    replaceAll(html, placeholderData, data);
    replaceAll(html, placeholderCount, std::to_string(emitted));
    replaceAll(html, placeholderGen, nowTimestamp());

    std::ofstream out(htmlPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    // 写 UTF-8 BOM：Windows 上某些浏览器/编辑器会按系统默认编码（GBK）猜测，
    // 带上 BOM 后一律按 UTF-8 解析，避免打开即乱码。
    out << "\xEF\xBB\xBF";
    out << html;
    out.flush();
    return out.good();
}

}  // namespace report

}  // namespace ros_sim
