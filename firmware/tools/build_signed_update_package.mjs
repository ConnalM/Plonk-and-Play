#!/usr/bin/env node
// Build a signed PPU1 package from one controlled update-identity JSON file.
// The signing key is supplied only by the protected local build environment.

import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';

const usage = 'usage: build_signed_update_package.mjs --identity <identity.json> --firmware <firmware.bin> --private-key <private.pem> --output <update.ppu>';
const args = Object.create(null);
for (let index = 2; index < process.argv.length; index += 2) {
  const key = process.argv[index];
  const value = process.argv[index + 1];
  if (!key?.startsWith('--') || !value) throw new Error(usage);
  args[key.slice(2)] = value;
}
for (const key of ['identity', 'firmware', 'private-key', 'output']) if (!args[key]) throw new Error(usage);

const identity = JSON.parse(fs.readFileSync(args.identity, 'utf8'));
if (Object.keys(identity).sort().join(',') !== 'board,build_id,product,version' ||
    identity.product !== 'plonk-and-play' || identity.board !== 'esp32-8048s070' ||
    !Number.isInteger(identity.version) || identity.version < 1 || identity.version > 0xffffffff ||
    !/^[\x21-\x7e]{1,31}$/.test(identity.build_id)) {
  throw new Error('invalid controlled update identity');
}

const product = Buffer.from(identity.product, 'ascii');
const board = Buffer.from(identity.board, 'ascii');
const build = Buffer.from(identity.build_id, 'ascii');
const payload = fs.readFileSync(args.firmware);
if (!payload.length) throw new Error('firmware payload is empty');
const digest = crypto.createHash('sha256').update(payload).digest();
const privateKey = fs.readFileSync(args['private-key'], 'utf8');
const prefixBytes = 52;
let signatureSize = 72;
let signature = Buffer.alloc(0);
let unsigned = Buffer.alloc(0);
for (let attempt = 0; attempt < 32; ++attempt) {
  const headerSize = prefixBytes + product.length + board.length + build.length + signatureSize;
  const prefix = Buffer.alloc(prefixBytes);
  prefix.write('PPU1', 0, 'ascii');
  prefix[4] = 1;
  prefix.writeUInt16LE(headerSize, 5);
  prefix[7] = product.length; prefix[8] = board.length; prefix[9] = build.length;
  prefix.writeUInt32LE(identity.version, 10);
  prefix.writeUInt32LE(payload.length, 14);
  digest.copy(prefix, 18);
  prefix.writeUInt16LE(signatureSize, 50);
  unsigned = Buffer.concat([prefix, product, board, build]);
  signature = crypto.sign('sha256', unsigned, { key: privateKey, dsaEncoding: 'der' });
  if (signature.length === signatureSize) break;
  signatureSize = signature.length;
  if (attempt === 31) throw new Error('ECDSA signature length did not stabilise');
}
const packageBytes = Buffer.concat([unsigned, signature, payload]);
fs.mkdirSync(path.dirname(args.output), { recursive: true });
fs.writeFileSync(args.output, packageBytes);
console.log(JSON.stringify({
  format: 'PPU1', identity, payloadBytes: payload.length,
  payloadSha256: digest.toString('hex').toUpperCase(),
  packageSha256: crypto.createHash('sha256').update(packageBytes).digest('hex').toUpperCase(),
}, null, 2));
