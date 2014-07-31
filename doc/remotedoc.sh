#!/bin/sh
echo -e "help\nquit" \
	| xjadeo -R \
	| grep "@800 +  " \
	| sed 's%>%~!~%g' \
	| sed 's%<%\&nbsp\;<code><em>%g;s%~!~%</em></code>\&nbsp\;%g' \
	| sed 's%\[%\&nbsp\;<code>[%g;s%\]%]</code>\&nbsp\;%g' \
	| sed 's%:%</dt><dd>%;s%$%</dd>%;s%^@800 +  %<dt>%' \
	> /tmp/xjrem.html
