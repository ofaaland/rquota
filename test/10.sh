#!/bin/sh

cat >x.conf <<EOT
/foo:test:nothing:0
EOT
$PATH_REPQUOTA -b 1m -f x.conf -u 100-102,204-106 /foo
