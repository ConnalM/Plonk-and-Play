const crypto = require('crypto');
const fs = require('fs');

const root = process.cwd();
const envelopeSource = fs.readFileSync('firmware/include/pp/signed_update_envelope.h', 'utf8');
const adapterSource = fs.readFileSync('firmware/include/pp/esp32_firmware_update_adapter.h', 'utf8');
if (!envelopeSource.includes('PPU1') || !envelopeSource.includes('mbedtls_pk_verify')) throw new Error('signed envelope verifier missing');
if (!adapterSource.includes('verifier_.consume') || !adapterSource.includes('verifier_.finish')) throw new Error('adapter bypasses signed envelope');
if (!adapterSource.includes('const SignedEnvelopeResult envelopeResult = verifier_.finish(envelope)') ||
    !adapterSource.includes('candidate.authenticated = true')) throw new Error('adapter does not gate authenticity on envelope verification');
for (const path of ['firmware', 'acceptance', 'docs']) {
  const text = fs.readdirSync(path, { withFileTypes: true }).filter(e => e.isFile()).map(e => fs.readFileSync(`${path}/${e.name}`, 'utf8')).join('\n');
  if (text.includes('BEGIN EC PRIVATE KEY') || text.includes('BEGIN PRIVATE KEY')) throw new Error(`private key material committed under ${path}`);
}

const { privateKey, publicKey } = crypto.generateKeyPairSync('ec', { namedCurve: 'prime256v1' });
const payload = Buffer.from('deterministic P&P firmware payload');
const product = Buffer.from('plonk-and-play');
const board = Buffer.from('esp32-8048s070');
const build = Buffer.from('test-build');
const digest = crypto.createHash('sha256').update(payload).digest();
const prefixBytes = 52;
let signatureSize = 72;
let headerSize = prefixBytes + product.length + board.length + build.length + signatureSize;
const prefix = Buffer.alloc(prefixBytes);
prefix.write('PPU1', 0, 'ascii'); prefix[4] = 1; prefix.writeUInt16LE(headerSize, 5);
prefix[7] = product.length; prefix[8] = board.length; prefix[9] = build.length;
prefix.writeUInt32LE(42, 10); prefix.writeUInt32LE(payload.length, 14); digest.copy(prefix, 18); prefix.writeUInt16LE(signatureSize, 50);
let unsigned = Buffer.concat([prefix, product, board, build]);
let signature = crypto.sign('sha256', unsigned, { key: privateKey, dsaEncoding: 'der' });
if (signature.length > 80) throw new Error('test signature exceeded envelope bound');
signatureSize = signature.length;
headerSize = prefixBytes + product.length + board.length + build.length + signatureSize;
for (let attempt = 0; attempt < 32; ++attempt) {
  prefix.writeUInt16LE(headerSize, 5); prefix.writeUInt16LE(signatureSize, 50);
  unsigned = Buffer.concat([prefix, product, board, build]);
  signature = crypto.sign('sha256', unsigned, { key: privateKey, dsaEncoding: 'der' });
  if (signature.length === signatureSize) break;
  signatureSize = signature.length;
  headerSize = prefixBytes + product.length + board.length + build.length + signatureSize;
  if (attempt === 31) throw new Error('ECDSA signature length did not stabilise');
}
const packageBytes = Buffer.concat([unsigned, signature, payload]);
const verify = (candidate) => {
  if (candidate.length < prefixBytes || candidate.subarray(0, 4).toString('ascii') !== 'PPU1' || candidate[4] !== 1) return false;
  const candidateHeaderSize = candidate.readUInt16LE(5);
  const candidateImageSize = candidate.readUInt32LE(14);
  const candidateSignatureSize = candidate.readUInt16LE(50);
  if (candidateHeaderSize < prefixBytes || candidateHeaderSize + candidateImageSize !== candidate.length || candidateSignatureSize === 0 || candidateHeaderSize !== unsigned.length + candidateSignatureSize) return false;
  const candidateSignature = candidate.subarray(candidateHeaderSize - candidateSignatureSize, candidateHeaderSize);
  const candidatePayload = candidate.subarray(candidateHeaderSize);
  if (candidatePayload.length !== candidateImageSize) return false;
  if (!crypto.timingSafeEqual(crypto.createHash('sha256').update(candidatePayload).digest(), digest)) return false;
  return crypto.verify('sha256', candidate.subarray(0, candidateHeaderSize - candidateSignatureSize), publicKey, candidateSignature);
};
if (!verify(packageBytes)) throw new Error('correctly signed package rejected');
const mutations = [
  Buffer.from(packageBytes), Buffer.from(packageBytes), Buffer.from(packageBytes),
  Buffer.from(packageBytes), Buffer.from(packageBytes), Buffer.from(packageBytes),
];
mutations[0][10] ^= 1; // version
mutations[1][18] ^= 1; // declared digest
mutations[2][headerSize - 1] ^= 1; // signature
mutations[3][headerSize] ^= 1; // firmware byte
mutations[4] = packageBytes.subarray(0, packageBytes.length - 1); // truncated
mutations[5][14] = 0; // declared image size
for (const [index, candidate] of mutations.entries()) if (verify(candidate)) throw new Error(`mutation ${index} unexpectedly accepted`);
console.log('SIGNED_UPDATE_ENVELOPE_CONTRACT_PASS');
