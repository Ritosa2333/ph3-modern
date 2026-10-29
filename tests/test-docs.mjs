import assert from 'node:assert/strict';
import {mkdtempSync, mkdirSync, writeFileSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {resolve, sep} from 'node:path';
import {checkLinks} from '../tools/docs.mjs';

const directory = mkdtempSync(resolve(tmpdir(), 'ph3-document-links-'));
try {
  mkdirSync(resolve(directory, 'docs'));
  writeFileSync(resolve(directory, 'README.md'), '# 项目\n[指南](docs/guide.md#重复-1)\n');
  writeFileSync(resolve(directory, 'docs/guide.md'), '# 指南\n## 重复\n## 重复\n[项目](../README.md#项目)\n');
  assert.deepEqual(checkLinks(directory, ['README.md', 'docs/guide.md']), []);
  writeFileSync(resolve(directory, 'README.md'), '# 项目\n[缺失](docs/guide.md#缺失)\n[文件](missing.md)\n[越界](../outside.md)\n```sh\n[示例](unused.md)\n```\n');
  const issues = checkLinks(directory, ['README.md']);
  assert.equal(issues.length, 3, issues.join('\n'));
  assert.ok(issues.some(issue => issue.includes('missing heading')));
  assert.ok(issues.every(issue => !issue.includes('unused.md')));
  console.log('PASS Unicode and duplicate headings, missing links, path boundaries and fenced examples');
} finally {
  assert.ok(directory.startsWith(resolve(tmpdir()) + sep));
  rmSync(directory, {recursive: true, force: true});
}
