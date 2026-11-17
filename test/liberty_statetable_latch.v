module top (cp0, cp1, d, d0, d1, s0, s1, q, q_sep);
  input cp0, cp1, d, d0, d1, s0, s1;
  output q, q_sep;

  LATCH_ARRAY u1 (.CP0(cp0), .CP1(cp1), .D(d), .S0(s0), .S1(s1), .Q(q));
  LATCH_ARRAY_SEP u2 (.CP0(cp0), .CP1(cp1), .D0(d0), .D1(d1), .S0(s0), .S1(s1), .Q(q_sep));
endmodule
