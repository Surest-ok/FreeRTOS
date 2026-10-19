#!/usr/bin/env python3
"""Export the editable diagram models as Dawn-style SVG and offline HTML."""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path
import unicodedata
import xml.etree.ElementTree as ET


HERE = Path(__file__).resolve().parent
SCALE = 1.4
TOP = 64
FLOW_NAMES = frozenset(('boot', 'testing'))
PALETTES = {
    'dark': {
        'bg': '#0d1117', 'text': '#e6edf3', 'muted': '#9198a1',
        'layer': '#151b23', 'border': '#3d444d',
        'app': '#0f2d1a', 'app-stroke': '#2ea043',
        'ker': '#0c2d4a', 'ker-stroke': '#388bfd',
        'vir': '#2a1740', 'vir-stroke': '#a371f7',
        'hw': '#2e2405', 'hw-stroke': '#bb8009',
    },
    'light': {
        'bg': '#ffffff', 'text': '#1f2328', 'muted': '#59636e',
        'layer': '#f6f8fa', 'border': '#d1d9e0',
        'app': '#dafbe1', 'app-stroke': '#4ac26b',
        'ker': '#ddf4ff', 'ker-stroke': '#54aeff',
        'vir': '#fbefff', 'vir-stroke': '#c297ff',
        'hw': '#fff8c5', 'hw-stroke': '#d4a72c',
    },
}


def esc(value):
    return html.escape(str(value), quote=True)


def number(value):
    return f'{value:.2f}'.rstrip('0').rstrip('.')


def css():
    base = '''
.dawn-diagram text{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",
"Microsoft YaHei","Noto Sans CJK SC",sans-serif;fill:var(--text)}
.bg{fill:var(--bg)}
.layer{fill:var(--layer);stroke:var(--border);stroke-width:1.2}
.tag{font-size:15px;font-weight:600;fill:var(--muted)!important}
.title{font-size:23px;font-weight:650}
.node-title{font-size:16px;font-weight:600}
.node-sub{font-size:13.5px;fill:var(--muted)!important}
.app{fill:var(--app);stroke:var(--app-stroke)}
.ker{fill:var(--ker);stroke:var(--ker-stroke)}
.vir{fill:var(--vir);stroke:var(--vir-stroke)}
.hw{fill:var(--hw);stroke:var(--hw-stroke)}
.node-box{stroke-width:1.5}
.flow{fill:none;stroke:var(--muted);stroke-width:1.8;
stroke-linecap:round;stroke-linejoin:round}
.arrow-head{fill:var(--muted)}
.edge-label{font-size:13px;fill:var(--muted)!important}
'''
    for key, value in PALETTES['dark'].items():
        base = base.replace(f'var(--{key})', value)
    light = PALETTES['light']
    prefix = '.dawn-diagram[data-theme="light"] '
    overrides = [f'{prefix}text{{fill:{light["text"]}}}',
                 f'{prefix}.bg{{fill:{light["bg"]}}}',
                 f'{prefix}.layer{{fill:{light["layer"]};stroke:{light["border"]}}}']
    for name in ('app', 'ker', 'vir', 'hw'):
        overrides.append(f'{prefix}.{name}{{fill:{light[name]};'
                         f'stroke:{light[name+"-stroke"]}}}')
    for name in ('tag', 'node-sub', 'edge-label'):
        overrides.append(f'{prefix}.{name}{{fill:{light["muted"]}!important}}')
    overrides.extend([f'{prefix}.flow{{stroke:{light["muted"]}}}',
                      f'{prefix}.arrow-head{{fill:{light["muted"]}}}'])
    return base + '\n'.join(overrides)


def text_width(text, font):
    return sum(font * (1.0 if unicodedata.east_asian_width(c) in 'WF'
                       else 0.60) for c in text)


def wrap(text, width, font):
    lines = []
    for paragraph in text.split('\n'):
        line = ''
        for char in paragraph:
            if line and text_width(line + char, font) > width:
                lines.append(line.rstrip())
                line = char.lstrip()
            else:
                line += char
        if line:
            lines.append(line.rstrip())
    return lines


def xy(point, top=TOP):
    return point[0] * SCALE, point[1] * SCALE + top


def rect(node, top=TOP):
    x, y = xy(node['pos'], top)
    return x, y, node['size'][0] * SCALE, node['size'][1] * SCALE


def anchor(node, side, top=TOP):
    x, y, w, h = rect(node, top)
    return {'left': (x, y + h / 2), 'right': (x + w, y + h / 2),
            'top': (x + w / 2, y), 'bottom': (x + w / 2, y + h)}[side]


def node_style(node):
    if 'cherry_kal' in node['label']:
        return 'vir'
    return {'frontend': 'app', 'external': 'hw', 'security': 'vir'}.get(
        node['type'], 'ker')


def draw_svg(model, stem):
    nodes = model['components']
    by_id = {node['id']: node for node in nodes}
    connections = model.get('connections', [])
    is_flow = stem in FLOW_NAMES
    if bool(connections) != is_flow:
        raise ValueError(f'{stem}: architecture must be arrow-free; flows need arrows')
    top = (106 if model.get('boundaries') else 80) - min(n['pos'][1]*SCALE for n in nodes)
    width = max(rect(n, top)[0] + rect(n, top)[2] for n in nodes) + 48
    height = max(rect(n, top)[1] + rect(n, top)[3] for n in nodes) + 44
    title = model['meta']['title']
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" '
             f'width="{number(width)}" height="{number(height)}" '
             f'viewBox="0 0 {number(width)} {number(height)}" '
             f'role="img" aria-label="{esc(title)}" '
             f'class="dawn-diagram" data-theme="dark" '
             f'data-kind="{"workflow" if is_flow else "architecture"}">',
             f'<title>{esc(title)}</title>', f'<style>{css()}</style>',
             '<rect class="bg" width="100%" height="100%"/>',
             f'<text class="title" x="32" y="36">{esc(title)}</text>']
    if is_flow:
        parts.append(f'<defs><marker id="arrow-{stem}" viewBox="0 0 10 10" '
                     'refX="9" refY="5" markerWidth="7" markerHeight="7" '
                     'orient="auto-start-reverse"><path class="arrow-head" '
                     'd="M0,0 L10,5 L0,10 z"/></marker></defs>')
    for group in model.get('boundaries', []):
        boxes = [rect(by_id[n], top) for n in group['wraps']]
        x = min(b[0] for b in boxes) - 18
        y = min(b[1] for b in boxes) - 42
        w = max(b[0] + b[2] for b in boxes) - x + 18
        h = max(b[1] + b[3] for b in boxes) - y + 18
        parts.append(f'<rect class="layer" x="{number(x)}" y="{number(y)}" '
                     f'width="{number(w)}" height="{number(h)}" rx="11"/>')
        parts.append(f'<text class="tag" x="{number(x+14)}" '
                     f'y="{number(y+25)}">{esc(group["label"])}</text>')
    for edge in connections:
        source, target = by_id[edge['from']], by_id[edge['to']]
        points = [anchor(source, edge.get('fromSide', 'right'), top)]
        points.extend(xy(p, top) for p in edge.get('via', []))
        points.append(anchor(target, edge.get('toSide', 'left'), top))
        path = 'M' + ' L'.join(f'{number(x)},{number(y)}' for x, y in points)
        dashed = ' stroke-dasharray="6 5"' if edge.get('variant') == 'dashed' else ''
        parts.append(f'<path class="flow" data-edge="{esc(edge["id"])}" '
                     f'd="{path}" marker-end="url(#arrow-{stem})"{dashed}/>')
        if edge.get('label'):
            px, py = xy(edge['labelAt'], top)
            parts.append(f'<text class="edge-label" x="{number(px)}" '
                         f'y="{number(py-8)}" text-anchor="middle">'
                         f'{esc(edge["label"])}</text>')
    for node in nodes:
        x, y, w, h = rect(node, top)
        label = wrap(node['label'], w - 28, 16)
        subtitle = wrap(node.get('sublabel', ''), w - 28, 13.5)
        content_h = len(label) * 23 + len(subtitle) * 19 + (7 if subtitle else 0)
        if content_h + 20 > h:
            raise ValueError(f'{stem}/{node["id"]}: text needs {content_h+20}px; box is {h}px')
        parts.append(f'<g data-node-id="{esc(node["id"])}" '
                     f'data-label="{esc(node["label"])}" '
                     f'data-sublabel="{esc(node.get("sublabel", ""))}">')
        parts.append(f'<rect class="node-box {node_style(node)}" '
                     f'x="{number(x)}" y="{number(y)}" width="{number(w)}" '
                     f'height="{number(h)}" rx="9"/>')
        baseline = y + (h - content_h) / 2 + 18
        for line in label:
            parts.append(f'<text class="node-title" x="{number(x+w/2)}" '
                         f'y="{number(baseline)}" text-anchor="middle">{esc(line)}</text>')
            baseline += 23
        if subtitle:
            baseline += 3
        for line in subtitle:
            parts.append(f'<text class="node-sub" x="{number(x+w/2)}" '
                         f'y="{number(baseline)}" text-anchor="middle">{esc(line)}</text>')
            baseline += 19
        parts.append('</g>')
    parts.append('</svg>')
    result = '\n'.join(parts) + '\n'
    ET.fromstring(result)
    return result


def draw_html(model, stem, svg):
    title = esc(model['meta']['title'])
    return f'''<!doctype html>
<html lang="zh-CN"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>{title}</title>
<style>
body{{margin:0;background:#0d1117;color:#e6edf3;font:14px "Segoe UI","Microsoft YaHei",sans-serif}}
body[data-theme="light"]{{background:#fff;color:#1f2328}}
nav{{display:flex;gap:16px;align-items:center;padding:14px 24px;border-bottom:1px solid #3d444d}}
a{{color:#58a6ff}}button{{color:inherit;background:transparent;border:1px solid #6e7781;
border-radius:6px;padding:6px 12px;cursor:pointer}}
main{{max-width:1440px;margin:auto;padding:16px}}svg{{display:block;width:100%;height:auto}}
@media print{{nav{{display:none}}main{{padding:0}}}}
</style>
<body data-theme="dark"><nav><strong>{title}</strong>
<button type="button" id="theme">切换深浅色</button>
<a href="../images/{stem}.svg" download>下载 SVG</a>
<a href="{stem}.archify.json">可编辑 JSON</a></nav>
<main>{svg}</main>
<script>
document.getElementById('theme').addEventListener('click', () => {{
  const theme = document.body.dataset.theme === 'dark' ? 'light' : 'dark';
  document.body.dataset.theme = theme;
  document.querySelector('svg').dataset.theme = theme;
}});
</script></body></html>
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='verify exports without writing')
    args = parser.parse_args()
    for path in sorted(HERE.glob('*.archify.json')):
        stem = path.name[:-len('.archify.json')]
        model = json.loads(path.read_text(encoding='utf-8'))
        svg = draw_svg(model, stem)
        outputs = {HERE.parent / 'images' / f'{stem}.svg': svg,
                   HERE / f'{stem}.html': draw_html(model, stem, svg)}
        for output, content in outputs.items():
            if args.check:
                if output.read_text(encoding='utf-8') != content:
                    raise ValueError(f'stale diagram export: {output.name}')
            else:
                with output.open('w', encoding='utf-8', newline='\n') as stream:
                    stream.write(content)
        print(f'{stem}: {len(model["components"])} nodes, '
              f'{len(model.get("connections", []))} arrows')


if __name__ == '__main__':
    main()
