#!/usr/bin/env node
// Offline release gate: a signed PPU1 manifest must exactly describe its ESP
// payload. This detects an otherwise valid package/image pair with divergent
// app-descriptor identity before a controller can stage it.

import childProcess from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const args = Object.create(null);
for (let index = 2; index < process.argv.length; index += 2) {
  const key = process.argv[index];
  const value = process.argv[index + 1];
  if (!key?.startsWith('--') || !value) throw new Error('arguments must be --name value pairs');
  args[key.slice(2)] = value;
}

const canonical = { product: 'plonk-and-play', board: 'esp32-8048s070' };
function parsePackage(bytes) {
  if (bytes.length < 52 || bytes.subarray(0, 4).toString('ascii') !== 'PPU1' || bytes[4] !== 1) throw new Error('PPU1 envelope is malformed');
  const headerSize = bytes.readUInt16LE(5), productLength = bytes[7], boardLength = bytes[8], buildLength = bytes[9];
  const version = bytes.readUInt32LE(10), imageSize = bytes.readUInt32LE(14), signatureSize = bytes.readUInt16LE(50);
  if (!headerSize || headerSize > 256 || !signatureSize || headerSize + imageSize !== bytes.length || 52 + productLength + boardLength + buildLength + signatureSize !== headerSize) throw new Error('PPU1 envelope layout is invalid');
  let offset = 52;
  const product = bytes.subarray(offset, offset += productLength).toString('ascii');
  const board = bytes.subarray(offset, offset += boardLength).toString('ascii');
  const buildId = bytes.subarray(offset, offset += buildLength).toString('ascii');
  const signed = bytes.subarray(0, headerSize - signatureSize), signature = bytes.subarray(headerSize - signatureSize, headerSize), payload = bytes.subarray(headerSize);
  const declaredDigest = bytes.subarray(18, 50), actualDigest = crypto.createHash('sha256').update(payload).digest();
  if (!crypto.timingSafeEqual(declaredDigest, actualDigest)) throw new Error('PPU1 payload digest mismatch');
  return { product, board, buildId, version, imageSize, signed, signature, payload, payloadSha256: actualDigest.toString('hex').toUpperCase() };
}
function imageInfo(image, tool) {
  const command = tool || path.join(process.env.USERPROFILE || '', '.platformio', 'packages', 'tool-esptool', 'esptool.exe');
  const output = childProcess.execFileSync(command, ['--chip', 'esp32s3', 'image-info', image], { encoding: 'utf8' });
  const match = label => {
    const found = output.match(new RegExp(`^${label}:\\s*(.+)$`, 'mi'));
    if (!found) throw new Error(`ESP image-info has no ${label}`);
    return found[1].trim();
  };
  if (!/Checksum:\s+0x[0-9a-f]+\s+\(valid\)/i.test(output)) throw new Error('ESP image checksum is invalid');
  if (!/Validation hash:.*\(valid\)/i.test(output)) throw new Error('ESP image validation hash is invalid');
  return { product: match('Project name'), buildId: match('App version'), version: Number(match('Secure version')), raw: output };
}
function assertIdentity(manifest, descriptor) {
  if (manifest.product !== canonical.product || manifest.board !== canonical.board) throw new Error('PPU1 product or board is not canonical');
  if (descriptor.product !== canonical.product) throw new Error('ESP image project identity is not canonical');
  const differences = [];
  if (manifest.product !== descriptor.product) differences.push('product');
  if (manifest.buildId !== descriptor.buildId) differences.push('build ID');
  if (manifest.version !== descriptor.version) differences.push('version / secure_version');
  if (differences.length) throw new Error(`PPU1/image identity mismatch: ${differences.join(', ')}`);
}
function requireArgs(...names) { for (const name of names) if (!args[name]) throw new Error(`missing --${name}`); }

if (args['self-test']) {
  const valid = { product: 'plonk-and-play', board: 'esp32-8048s070', buildId: 'candidate-B', version: 101 };
  assertIdentity(valid, { product: 'plonk-and-play', buildId: 'candidate-B', version: 101 });
  let rejected = false;
  try { assertIdentity(valid, { product: 'plonk-and-play', buildId: 'wp6-physical-b', version: 1 }); } catch (_) { rejected = true; }
  if (!rejected) throw new Error('identity mismatch self-test was accepted');
  console.log('UPDATE_PACKAGE_IMAGE_IDENTITY_CONTRACT_SELF_TEST_PASS');
  process.exit(0);
}

requireArgs('package', 'image');
const packageBytes = fs.readFileSync(args.package), manifest = parsePackage(packageBytes);
const imageBytes = fs.readFileSync(args.image);
if (!manifest.payload.equals(imageBytes) || manifest.imageSize !== imageBytes.length) throw new Error('PPU1 payload is not the supplied final firmware image');
if (args['public-key']) {
  const publicKey = fs.readFileSync(args['public-key'], 'utf8');
  if (!crypto.verify('sha256', manifest.signed, publicKey, manifest.signature)) throw new Error('PPU1 signature is invalid');
}
const descriptor = imageInfo(args.image, args.esptool);
assertIdentity(manifest, descriptor);
console.log(JSON.stringify({ contract: 'PASS', manifest: { product: manifest.product, board: manifest.board, buildId: manifest.buildId, version: manifest.version, payloadSha256: manifest.payloadSha256 }, descriptor: { product: descriptor.product, buildId: descriptor.buildId, secureVersion: descriptor.version } }, null, 2));
