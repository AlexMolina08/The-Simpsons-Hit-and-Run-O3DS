import {createBuildStorage} from '../opfs.mjs';
const status = document.getElementById('status');
try {
  const storage = await createBuildStorage(), record = await storage.readJSON('completed.json');
  const cia = await storage.read('installer.cia'), report = await storage.read('build-report.json');
  if (record?.version !== '1.2' || !cia || !report || cia.size !== record.bytes) throw new Error('No complete v1.2 build exists on this origin.');
  for (const [file, name, label] of [[report, 'Hit-Run-3DS-build-report.json', 'Download build report'], [cia, record.filename, 'Download completed CIA']]) {
    const link = document.createElement('a'); link.href = URL.createObjectURL(file); link.download = name; link.textContent = label;
    document.getElementById('links').append(link, document.createElement('br'));
  }
  status.textContent = `${record.filename}: ${cia.size.toLocaleString()} bytes`;
  document.getElementById('report').textContent = await report.text();
} catch (error) { status.textContent = error.message; }
