#!/bin/sh
mkdir -p build
keytool -genkeypair -validity 1000 -dname "CN=kototuz,O=Android,C=ES" -keystore build/kototuz.keystore -storepass 'kototuz' -keypass 'kototuz' -alias projectKey -keyalg RSA
