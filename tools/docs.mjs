import {spawnSync} from 'node:child_process';
import {existsSync, readFileSync, writeFileSync} from 'node:fs';
import {dirname, relative, resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {root} from './lib/project.mjs';

function prose(text) {
  return text.replace(/^\s*(`{3,}|~{3,})[^\n]*\n[\s\S]*?^\s*\1\s*$/gm, '');
}

function anchors(text) {
  const counts = new Map();
  return new Set([...prose(text).matchAll(/^#{1,6}\s+(.+)$/gm)].map(([, title]) => {
    const base = title.trim().toLowerCase().replace(/<[^>]+>/g, '')
      .replace(/[^\p{L}\p{N}\p{M}\s_-]/gu, '').replace(/\s/g, '-');
    const count = counts.get(base) || 0;
    counts.set(base, count + 1);
    return base + (count ? '-' + count : '');
  }));
}

export function checkLinks(directory, documents) {
  const issues = [];
  for (const path of documents) {
    const source = readFileSync(resolve(directory, path), 'utf8');
    if (source.includes('\uFFFD')) issues.push(`${path}: invalid UTF-8`);
    for (const [, destination] of prose(source).matchAll(/!?\[[^\]\n]*\]\(([^)\n]+)\)/g)) {
      const link = destination.trim().replace(/^<|>$/g, '').split(/\s+"/)[0];
      if (/^(?:https?:|mailto:)/i.test(link)) continue;
      let decoded;
      try { decoded = decodeURIComponent(link); }
      catch { issues.push(`${path}: invalid URL ${link}`); continue; }
      const [file, fragment] = decoded.split('#');
      const target = resolve(dirname(resolve(directory, path)), file || '');
      const actual = file ? target : resolve(directory, path);
      const rel = relative(directory, actual);
      if (rel.startsWith('..') || /^[A-Za-z]:/.test(rel) || !existsSync(actual)) {
        issues.push(`${path}: missing or external local link ${link}`);
      } else if (fragment && actual.endsWith('.md') &&
                 !anchors(readFileSync(actual, 'utf8')).has(fragment)) {
        issues.push(`${path}: missing heading ${link}`);
      }
    }
  }
  return issues;
}

export function checkDocumentation() {
  const git = spawnSync('git', ['ls-files', '--cached', '--others', '--exclude-standard', '-z'],
    {cwd: root, encoding: 'utf8', maxBuffer: 16 * 1024 * 1024});
  if (git.status !== 0 || git.error) throw new Error(git.error?.message || git.stderr);
  const files = [...new Set(git.stdout.split('\0'))]
    .filter(path => path.endsWith('.md') && existsSync(resolve(root, path))).sort();
  const indexPath = 'docs/INDEX.md';
  const index = '# 文档总表\n\n| 文档 | 内容 |\n| --- | --- |\n' + files
    .filter(path => path !== indexPath).map(path => {
      const title = readFileSync(resolve(root, path), 'utf8').match(/^#\s+(.+)$/m)?.[1] || path;
      return `| [${path}](../${path}) | ${title.replaceAll('|', '\\|')} |`;
    }).join('\n') + '\n';
  if (process.argv.includes('--write-index')) writeFileSync(resolve(root, indexPath), index);
  const issues = checkLinks(root, files);
  if (!existsSync(resolve(root, indexPath)) || readFileSync(resolve(root, indexPath), 'utf8') !== index)
    issues.push('Run npm run docs:index to refresh docs/INDEX.md.');
  if (issues.length) throw new Error(issues.join('\n'));
  console.log(`PASS documentation index and local links (${files.length} documents)`);
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url)
  checkDocumentation();
