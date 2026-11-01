const fs = require('fs');

const ini = fs.readFileSync('firmware/platformio.ini', 'utf8');
const script = fs.readFileSync('firmware/app_descriptor_preimage.py', 'utf8');
if (!ini.includes('post:app_descriptor_preimage.py')) throw new Error('pre-image identity hook missing');
if (ini.includes('post:postbuild_app_identity.py')) throw new Error('completed-binary identity mutation remains configured');
if (!script.includes('IDENTITY_PRODUCT.encode("ascii")')) throw new Error('canonical product identity binding missing');
if (!script.includes('.flash.appdesc') || !script.includes('AddPostAction("$BUILD_DIR/${PROGNAME}.elf"')) {
  throw new Error('ELF app-descriptor pre-image hook missing');
}
if (!script.includes('load_update_identity_from_environment') || !script.includes('identity.version')) {
  throw new Error('update descriptor identity is not driven by the shared identity input');
}
if (script.includes('SECURE_VERSION_OFFSET, 1')) {
  throw new Error('obsolete hard-coded secure version remains');
}

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
