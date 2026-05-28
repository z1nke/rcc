// RUN: %check_rcc_pp_run %s %t

#
  #
/* comment */ #

// Ignore #pragma
#pragma once
#pragma GCC diagnostic ignored "-Wunused"

int main() { return 0; }
