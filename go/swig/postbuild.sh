#!/bin/bash

grep -q 'import \"math\"' $1 || sed -i '/import \"C\"/a import \"math\"' $1
cgoLine="#cgo LDFLAGS: -L. -lubiquityclientwrapper -lubiquity_client_static -lbsd -lcurl -luuid -lstdc++ -lm -ldl -lssl -lcrypto"
sed -i "s|#define intgo|$cgoLine\n\n#define intgo|g" $1
cat $2 >>$1