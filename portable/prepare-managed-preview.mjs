// Local-only integration fixture: preserve the existing Launcher preview and
// every published runtime generation. No upload or deployment is performed.
import {cp,mkdir,readdir,readFile,writeFile,access} from 'node:fs/promises';
import {resolve,dirname} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
const root=resolve(dirname(fileURLToPath(import.meta.url)),'..');
const flags=new Map(process.argv.slice(2).map(arg=>{const [k,...v]=arg.replace(/^--/,'').split('=');return[k,v.join('=')];}));
if(!flags.get('runtime')||!flags.get('out'))throw Error('Use --runtime=build/... --out=build/...');
const launcher=resolve(flags.get('launcher')||resolve(root,'../eagler-touhou-hsob'));
const source=resolve(flags.get('source')||resolve(launcher,'.cache/test-hsob-import-20260930'));
const build=resolve(root,flags.get('runtime')),site=resolve(root,flags.get('out'));
if(site===source||site===build)throw Error('Preview must use a new, separate directory');
try{await access(site);throw Error('Preview output already exists; use another --out label');}
catch(error){if(error.code!=='ENOENT')throw error;}
const {writeRuntimeGeneration,publishRuntimeManifest}=await import(pathToFileURL(resolve(launcher,'lib/runtime-generations.mjs')));
const {writeSiteMetadata}=await import(pathToFileURL(resolve(launcher,'lib/site-metadata.mjs')));
const directory=JSON.parse(await readFile(resolve(build,'runtime-files.json'),'utf8'));
await mkdir(site,{recursive:true});
for(const name of await readdir(source))
  if(!['runtime','runtime-manifest.json','.tmp'].includes(name))await cp(resolve(source,name),resolve(site,name),{recursive:true});
const current=await writeRuntimeGeneration({site,root:'runtime/hsob/',source:build,entry:'hsob.html',
  names:Object.keys(directory.files),expected:directory.files});
await publishRuntimeManifest(site,[{root:'runtime/hsob/',current}]);
const host=JSON.parse(await readFile(resolve(site,'host-manifest.json'),'utf8'));
host.games.hsob.runtime=`runtime/hsob/${current.generation}/hsob.html?hosted=1`;
await writeFile(resolve(site,'host-manifest.json'),JSON.stringify(host,null,2)+'\n');
const deployment=JSON.parse(await readFile(resolve(site,'deployment.json'),'utf8'));
deployment.files=deployment.files.filter(file=>!file.path.startsWith('runtime/'));
await writeFile(resolve(site,'deployment.json'),JSON.stringify(deployment,null,2)+'\n');
await writeSiteMetadata(site,deployment.siteUrl);
const receipt={sourceLauncher:source,sourceRuntime:build,generation:current.generation,
  build:JSON.parse(await readFile(resolve(build,'build.json'),'utf8')),localOnly:true};
await writeFile(resolve(site,'preview-build.json'),JSON.stringify(receipt,null,2)+'\n');
console.log(JSON.stringify({site,generation:current.generation,localOnly:true}));
