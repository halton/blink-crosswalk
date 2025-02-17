function importTestKeys()
{
    var keyFormat = "spki";
    var data = new Uint8Array([]);
    var extractable = true;
    var keyUsages = ['encrypt', 'decrypt', 'sign', 'verify'];

    var hmacPromise = crypto.subtle.importKey(keyFormat, data, {name: 'hmac', hash: {name: 'sha-1'}}, extractable, keyUsages);
    var rsaSsaPromise = crypto.subtle.importKey(keyFormat, data, {name: 'RSASSA-PKCS1-v1_5', hash: {name: 'sha-1'}}, extractable, keyUsages);
    var aesCbcPromise = crypto.subtle.importKey(keyFormat, data, {name: 'AES-CBC'}, extractable, keyUsages);
    var aesCbcJustDecrypt = crypto.subtle.importKey(keyFormat, data, {name: 'AES-CBC'}, false, ['decrypt']);

    return Promise.every(hmacPromise, rsaSsaPromise, aesCbcPromise, aesCbcJustDecrypt).then(function(results) {
        return {
            hmacSha1: results[0],
            rsaSsaSha1: results[1],
            aesCbc: results[2],
            aesCbcJustDecrypt: results[3],
        };
    });
}

// Builds a hex string representation of any array-like input (array or
// ArrayBufferView). The output looks like this:
//    [ab 03 4c 99]
function byteArrayToHexString(bytes)
{
    var hexBytes = [];

    for (var i = 0; i < bytes.length; ++i) {
        var byteString = bytes[i].toString(16);
        if (byteString.length < 2)
            byteString = "0" + byteString;
        hexBytes.push(byteString);
    }

    return "[" + hexBytes.join(" ") + "]";
}

function asciiToArrayBuffer(str)
{
    var chars = [];
    for (var i = 0; i < str.length; ++i)
        chars.push(str.charCodeAt(i));
    return new Uint8Array(chars);
}

function failAndFinishJSTest(error)
{
    if (error)
       debug(error);
    finishJSTest();
}
