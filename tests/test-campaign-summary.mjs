import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {mkdirSync,writeFileSync,readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root=fileURLToPath(new URL('../',import.meta.url));
const out=resolve(root,'build/campaign-summary-regression');
mkdirSync(out,{recursive:true});
const report={outcome:'requested-frame-reached',fixtureSha256:'fixture',errors:[],
  build:{variant:'standard'},samplingMethod:'browser-timer-v1',sampling:{dropped:0}};
const row=(sequence,callbackCount,callbackOverBudget,presentIntervalCount,missedRefreshCount)=>({
  time:sequence*1000,cpuThrottle:4,stage:{index:1,frame:8300+sequence*60},
  performance:{timingVersion:3,sequence,callbackCount,callbackOverBudget,
    presentIntervalCount,missedRefreshCount,windowMs:1000,logicHz:60}});
const run=(name,rows)=>{
  const path=resolve(out,name);mkdirSync(path,{recursive:true});
  writeFileSync(resolve(path,'report.json'),JSON.stringify(report));
  writeFileSync(resolve(path,'samples.jsonl'),rows.map(r=>JSON.stringify(r)+'\n').join(''));
  const summary=resolve(path,'summary.json');
  const result=spawnSync(process.env.PH3_PYTHON||'python',[
    'tools/summarize-campaign-runs.py',path,'--out='+summary],{cwd:root,encoding:'utf8'});
  assert.equal(result.status,0,result.stderr);
  return JSON.parse(readFileSync(summary,'utf8')).runs[0];
};
// Timer duplicates must not bias the rates. A tiny window must not weigh as
// much as a full one. CPU deadlines and missed display slots are independent.
const first=row(1,10,2,10,5),second=row(2,90,1,90,0);
const weighted=run('weighted',[first,first,second]);
assert.equal(weighted.timingComparable,true);
const counts=weighted.windows['stage1-boss'].deadline60Hz;
assert.equal(counts.windows,2);assert.equal(counts.callbackCount,100);
assert.equal(counts.callbackOverBudget,3);assert.equal(counts.cpuWithinBudgetPercent,97);
assert.equal(counts.deliveredRefreshPercent,100*100/105);assert.equal(counts.logicHz,60);
const missing=run('missing-window',[first,row(3,90,1,90,0)]);
assert.equal(missing.timingComparable,false);
assert.equal(missing.allSamples.deadline60Hz.missingWindows,1);
console.log('PASS weighted 60 Hz deadlines, independent presentation cadence, duplicate windows and missing evidence');
