const fs = require('fs');

const ini = fs.readFileSync('firmware/platformio.ini', 'utf8');
const script = fs.readFileSync('firmware/postbuild_app_identity.py', 'utf8');
if (!ini.includes('post:postbuild_app_identity.py')) throw new Error('production post-build identity hook missing');
if (!script.includes('CANONICAL_PRODUCT = b"plonk-and-play"')) throw new Error('canonical product constant missing');
if (!script.includes('ABCD5432') || !script.includes('hashlib.sha256')) throw new Error('ESP image assertion/hash repair missing');

const image = process.argv[2] || 'firmware/.pio/build/stage8bp1s3sd/firmware.bin';
if (fs.existsSync(image)) {
  const bytes = fs.readFileSync(image);
  const name = bytes.subarray(0x50, 0x70).toString('ascii').replace(/\0.*$/, '');
  if (name !== 'plonk-and-play') throw new Error(`production descriptor product is ${name || '<empty>'}`);
  if (name === 'arduino-lib-builder') throw new Error('obsolete Arduino project identity remains');
  console.log('ESP32 production app descriptor product: plonk-and-play');
} else {
  console.log('ESP32 production app descriptor hook: PASS (binary not present)');
}
