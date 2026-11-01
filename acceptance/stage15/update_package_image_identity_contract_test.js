const { execFileSync } = require('child_process');
const path = require('path');

execFileSync('node', [path.join('acceptance', 'stage15', 'update_package_image_identity_contract.mjs'), '--self-test', '1'], { stdio: 'inherit' });
console.log('UPDATE_PACKAGE_IMAGE_IDENTITY_CONTRACT_TEST_PASS');
