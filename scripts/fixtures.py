#!/usr/bin/env python3
"""Independent synthetic compatibility fixtures. Never use production secrets here."""
import base64, csv, hashlib, io, json, struct
from pathlib import Path
from urllib.parse import quote
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from argon2.low_level import hash_secret_raw, Type
from nacl.bindings import (crypto_secretstream_xchacha20poly1305_state,
    crypto_secretstream_xchacha20poly1305_init_push,
    crypto_secretstream_xchacha20poly1305_push,
    crypto_secretstream_xchacha20poly1305_TAG_FINAL)
import pyzipper, qrcode
ROOT = Path(__file__).resolve().parents[1] / 'tests/fixtures'
ROOT.mkdir(parents=True, exist_ok=True)
PW = 'synthetic päss🔐'.encode()
SECRET = 'GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ'
KEY = b'12345678901234567890'
SALT = b'0123456789abcdef'
NONCE = b'0123456789ab'
ISSUER, ACCOUNT = 'Étoile 日本', 'alice@example.test'
URI = f'otpauth://totp/{quote(ISSUER)}:{ACCOUNT}?secret={SECRET}&issuer={quote(ISSUER)}'
manifest = []
def js(o): return json.dumps(o, ensure_ascii=False, separators=(',', ':')).encode()
def b64(b): return base64.b64encode(b).decode()
def write(name, data, *, password=False, hint='auto', count=1, unsupported=0, fail=False, files=None):
    if not isinstance(data, bytes): data = js(data)
    (ROOT / name).write_bytes(data)
    manifest.append(dict(name=name, files=files or [name], password=password, hint=hint, count=count, unsupported=unsupported, fail=fail))
def encrypted(name, data, hint='auto'):
    write(name, data, password=True, hint=hint)
    manifest.append(dict(name=name+'-wrong-password',files=[name],password='wrong',hint=hint,fail=True))
def argon(memory=19*1024, ops=2): return hash_secret_raw(PW,SALT,ops,memory,1,32,Type.ID)
def varint(n):
    out=b''
    while n>127: out+=bytes([(n&127)|128]); n>>=7
    return out+bytes([n])
def integer(field, value): return varint(field<<3)+varint(value)
def blob(field, value): return varint((field<<3)|2)+varint(len(value))+value
def migration(index=0,size=1,type=2,id=17,version=1):
    a=blob(1,KEY)+blob(2,ACCOUNT.encode())+blob(3,ISSUER.encode())+integer(4,1)+integer(5,1)+integer(6,type)
    payload=blob(1,a)+integer(2,version)+integer(3,size)+integer(4,index)+integer(5,id)
    return 'otpauth-migration://offline?data='+quote(b64(payload),safe='')
entry={'type':'totp','name':ACCOUNT,'issuer':ISSUER,'info':{'secret':SECRET,'algo':'SHA1','digits':6,'period':30}}
db={'version':3,'entries':[entry]}
aegis={'version':1,'header':{'slots':None,'params':None},'db':db}
write('aegis.json',aegis)
master=bytes(range(32)); wrap=hashlib.scrypt(PW,salt=SALT,n=32768,r=8,p=1,maxmem=64*1024*1024,dklen=32)
wrapped=AESGCM(wrap).encrypt(NONCE,master,None); encrypted_db=AESGCM(master).encrypt(NONCE,js(db),None)
slot={'type':1,'n':32768,'r':8,'p':1,'salt':SALT.hex(),'key':wrapped[:-16].hex(),'key_params':{'nonce':NONCE.hex(),'tag':wrapped[-16:].hex()}}
encrypted('aegis-encrypted.json',{'version':1,'header':{'slots':[slot],'params':{'nonce':NONCE.hex(),'tag':encrypted_db[-16:].hex()}},'db':b64(encrypted_db[:-16])})
for version in range(1,5):
    services=[{'name':ISSUER,'secret':SECRET,'otp':{'account':ACCOUNT,'algorithm':'SHA1','digits':6,'period':30,'tokenType':'TOTP'}}]
    write(f'2fas-v{version}.2fas',{'schemaVersion':version,'services':services})
    k=hashlib.pbkdf2_hmac('sha256',PW,SALT,10000)
    parts=':'.join([b64(AESGCM(k).encrypt(NONCE,js(services),None)),b64(SALT),b64(NONCE)])
    encrypted(f'2fas-v{version}-encrypted.2fas',{'schemaVersion':version,'servicesEncrypted':parts})
raivo=[{'kind':'TOTP','issuer':ISSUER,'account':ACCOUNT,'secret':SECRET,'digits':'6','timer':'30','algorithm':'SHA1'}]
write('raivo.json',raivo)
buf=io.BytesIO()
with pyzipper.AESZipFile(buf,'w',compression=pyzipper.ZIP_DEFLATED,encryption=pyzipper.WZ_AES) as z:
    z.setpassword(PW); z.writestr('raivo-otp-export.json',js(raivo)); z.writestr('raivo-otp-export.html','synthetic')
encrypted('raivo.zip',buf.getvalue())
write('ente.txt',(URI+'\n').encode())
state=crypto_secretstream_xchacha20poly1305_state(); k=argon(64*1024,2)
header=crypto_secretstream_xchacha20poly1305_init_push(state,k)
cipher=crypto_secretstream_xchacha20poly1305_push(state,(URI+'\n').encode(),tag=crypto_secretstream_xchacha20poly1305_TAG_FINAL)
encrypted('ente-encrypted.json',{'version':1,'kdfParams':{'salt':b64(SALT),'opsLimit':2,'memLimit':64*1024*1024},'encryptedData':b64(cipher),'encryptionNonce':b64(header)})
bitwarden={'encrypted':False,'items':[{'name':ACCOUNT,'type':1,'login':{'totp':URI},'favorite':False}]}
write('bitwarden.json',bitwarden)
buf=io.StringIO(newline=''); writer=csv.writer(buf); writer.writerow(['folder','favorite','type','name','login_uri','login_totp']); writer.writerow(['','0','login','Alice, "日本"\nTeam','',URI]); write('bitwarden.csv',buf.getvalue().encode())
proton={'version':1,'entries':[{'content':{'name':ACCOUNT,'uri':URI}}]}
write('proton.json',proton)
encrypted('proton-encrypted.json',{'version':1,'salt':b64(SALT),'content':b64(NONCE+AESGCM(argon()).encrypt(NONCE,js(proton),b'proton.authenticator.export.v1'))})
andotp=[{'type':'TOTP','issuer':ISSUER,'label':ACCOUNT,'secret':SECRET,'algorithm':'SHA1','digits':6,'period':30}]
write('andotp.json',andotp)
k=hashlib.pbkdf2_hmac('sha1',PW,SALT[:12],10000,32)
encrypted('andotp-current.bin',struct.pack('>I',10000)+SALT[:12]+NONCE+AESGCM(k).encrypt(NONCE,js(andotp),None),hint='current')
encrypted('andotp-legacy.bin',NONCE+AESGCM(hashlib.sha256(PW).digest()).encrypt(NONCE,js(andotp),None),hint='legacy')
write('links.txt',(URI+'\n').encode())
write('google.txt',migration().encode())
write('google-multipart.txt',(migration(1,2)+'\n'+migration(0,2)).encode(),count=2)
(ROOT/'google-part1.txt').write_text(migration(0,2)); (ROOT/'google-part2.txt').write_text(migration(1,2))
manifest.append(dict(name='google-separate-files',files=['google-part2.txt','google-part1.txt'],count=2))
for ext in ('png','jpg'):
    qrcode.make(migration()).convert('RGB').save(ROOT/f'google.{ext}')
    manifest.append(dict(name=f'google.{ext}',files=[f'google.{ext}'],count=1))
for i in range(2): qrcode.make(migration(i,2)).save(ROOT/f'google-part{i+1}.png')
manifest.append(dict(name='google-two-images',files=['google-part2.png','google-part1.png'],count=2))
qrcode.make(URI).save(ROOT/'enrollment.png'); manifest.append(dict(name='enrollment-image-rejected',files=['enrollment.png'],fail=True))
write('unsupported.json',{'version':1,'db':{'version':3,'entries':[entry,dict(entry,type='hotp',name='Counter account'),dict(entry,type='motp',name='Mobile OTP')]}},unsupported=2)
write('steam.json',[dict(andotp[0],type='STEAM',digits=5)])
write('custom-period.txt',(URI+'&period=45&digits=8&algorithm=SHA512').encode())
write('corrupt-secret.json',{'version':1,'db':{'version':3,'entries':[entry,dict(entry,info={'secret':'BROKEN!','algo':'SHA1','digits':6,'period':30})]}},fail=True)
write('future-version.json',dict(aegis,version=999),fail=True)
write('future-2fas.json',{'schemaVersion':5,'services':services},fail=True)
write('empty.json',dict(aegis,db={'version':3,'entries':[]}),fail=True)
write('only-hotp.json',[dict(andotp[0],type='HOTP')],fail=True)
write('incomplete-google.txt',migration(0,2).encode(),fail=True)
write('duplicate-google.txt',(migration(0,2)+'\n'+migration(0,2)).encode(),fail=True)
write('mixed-google.txt',(migration(0,2)+'\n'+migration(1,2,id=99)).encode(),fail=True)
write('future-google.txt',migration(version=2).encode(),fail=True)
write('duplicate-param.txt',(URI+'&secret='+SECRET).encode(),fail=True)
write('invalid-period.txt',(URI+'&period=0').encode(),fail=True)
write('invalid-utf8.txt',URI.encode()+b'\xff',fail=True)
write('malformed.json',b'{"version":1,"db":',fail=True)
write('pgp.txt',b'-----BEGIN PGP MESSAGE-----\nsynthetic',fail=True)
write('vault-rejected.json',{'encrypted':False,'items':[{'name':'vault','login':{'totp':URI,'password':'not-a-real-password'}}]},fail=True)
# Actual Android/iOS export shape differences, independently synthesized.
write('bitwarden-ios.json',{'encrypted':False,'items':[{'name':ISSUER,'type':1,'login':{'totp':URI,'username':ACCOUNT}}]})
write('bitwarden-android.csv',('folder,favorite,type,name,login_uri,login_totp\n,,1,'+ISSUER+',,'+URI+','+ISSUER+',30,6\n').encode())
ios_header='folder,favorite,type,name,notes,fields,reprompt,login_uri,login_username,login_password,login_totp\n'
write('bitwarden-ios.csv',(ios_header+',,login,'+ISSUER+',,,0,,'+ACCOUNT+',,'+URI+'\n\n').encode())
write('query-spaces.txt',(f'otpauth://totp/A%20B:{ACCOUNT}?secret={SECRET}&issuer=A+B').encode())
write('proton-null-name.json',{'version':1,'entries':[{'content':{'name':None,'uri':URI,'entry_type':'Totp'}}]})
legacy_state=crypto_secretstream_xchacha20poly1305_state()
legacy_key=argon(64*1024,2)
legacy_header=crypto_secretstream_xchacha20poly1305_init_push(legacy_state,legacy_key)
legacy_cipher=crypto_secretstream_xchacha20poly1305_push(legacy_state,(URI+'\n').encode(),tag=0)
encrypted('ente-legacy-message.json',{'version':1,'kdfParams':{'salt':b64(SALT),'opsLimit':2,'memLimit':64*1024*1024},'encryptedData':b64(legacy_cipher),'encryptionNonce':b64(legacy_header)})
# Every encrypted adapter must fail on damaged bytes, without exposing plaintext.
for case in list(manifest):
    if case.get('password') is not True: continue
    name=case['files'][0]; damaged=bytearray((ROOT/name).read_bytes()); position=len(damaged)//2
    if name.endswith('.zip'): position=30+sum(struct.unpack_from('<HH',damaged,26))+20
    damaged[position]^=1
    write('corrupted-'+name,bytes(damaged),password=True,hint=case.get('hint','auto'),fail=True)
write('steam-link.txt',('otpauth://steam/Steam:'+ACCOUNT+'?secret='+SECRET).encode())
write('steam-invalid.txt',('otpauth://steam/Steam:'+ACCOUNT+'?secret='+SECRET+'&digits=8').encode(),fail=True)
(ROOT/'manifest.json').write_bytes(js(manifest))
print(f'{len(manifest)} synthetic cases generated')
