# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(print-name) begin
(print-name) Course : SSE3044
(print-name) ID     : 2011312805
(print-name) Name   : Jaehyun Song
(print-name) end
EOF
pass;
