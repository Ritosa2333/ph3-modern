// Own the read-only preview server for exactly one bounded campaign probe.
// This avoids detached-server timeouts between long verification passes.
import {spawn} from 'node:child_process';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = fileURLToPath(new URL('../', import.meta.url));
const args = process.argv.slice(2);
const runtime = args.find(arg => arg.startsWith('--runtime='))?.slice(10);
const port = Number(args.find(arg => arg.startsWith('--port='))?.slice(7) || 8873);
if (!runtime || !Number.isInteger(port) || port < 1024 || port > 65535)
  throw new Error('Expected --runtime=build/<variant> and optional --port=1024..65535');
if (args.some(arg => arg.startsWith('--url=')))
  throw new Error('This runner owns the local URL; pass --runtime instead');
const resourceArgs = args.filter(arg => arg.startsWith('--game-root=') || arg.startsWith('--font='));
const probeArgs = args.filter(arg => !['--runtime=', '--port=', '--game-root=', '--font=']
  .some(prefix => arg.startsWith(prefix)));
const server = spawn(process.env.PH3_PYTHON || 'python', ['-u', 'tools/serve-local.py',
  '--port', String(port), '--runtime', resolve(root, runtime), ...resourceArgs],
  {cwd: root, stdio: ['ignore', 'pipe', 'inherit'], windowsHide: true});
let probe;
const stop = () => {
  if (probe && probe.exitCode === null && !probe.killed) probe.kill();
  if (server.exitCode === null && !server.killed) server.kill();
};
process.on('exit', stop);
process.on('SIGINT', () => {stop(); process.exit(130);});
process.on('SIGTERM', () => {stop(); process.exit(143);});
try {
  await new Promise((accept, reject) => {
    let startup = '';
    const timer = setTimeout(() => reject(new Error('Preview server startup timed out')), 20000);
    server.on('error', error => {clearTimeout(timer); reject(error);});
    server.on('exit', (code, signal) => {
      clearTimeout(timer);
      reject(new Error(`Preview server exited before startup (${code ?? signal})`));
    });
    server.stdout.on('data', bytes => {
      process.stdout.write(bytes);
      startup = (startup + bytes.toString()).slice(-8192);
      if (startup.includes('Woo local preview:')) {clearTimeout(timer); accept();}
    });
  });
  probe = spawn(process.execPath, ['tools/profile-campaign.mjs',
    `--url=http://127.0.0.1:${port}/hsob.html`, ...probeArgs],
    {cwd: root, stdio: 'inherit', windowsHide: true});
  const code = await new Promise((accept, reject) => {
    probe.on('error', reject);
    probe.on('exit', (code, signal) => signal
      ? reject(new Error(`Campaign probe terminated by ${signal}`)) : accept(code));
  });
  process.exitCode = code === 0 ? 0 : (code || 1);
} finally {
  stop();
}
