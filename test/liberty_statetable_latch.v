module top (cp0, cp1, en, d, d0, d1, s0, s1, q, q_sep, q_when, q_sel, q_gated, q_gated2, q_low);
  input cp0, cp1, en, d, d0, d1, s0, s1;
  output q, q_sep, q_when, q_sel, q_gated, q_gated2, q_low;

  LATCH_ARRAY u1 (.CP0(cp0), .CP1(cp1), .D(d), .S0(s0), .S1(s1), .Q(q));
  LATCH_ARRAY_SEP u2 (.CP0(cp0), .CP1(cp1), .D0(d0), .D1(d1), .S0(s0), .S1(s1), .Q(q_sep));
  // Word 1 of u3, u4 and u6 is the one on cp0.
  LATCH_ARRAY_WHEN u3 (.CP0(cp1), .CP1(cp0), .D(d), .S0(s0), .S1(s1), .Q(q_when));
  LATCH_ARRAY_SEL u4 (.CP0(cp1), .CP1(cp0), .D(d), .S0(s0), .S1(s1), .Q(q_sel));
  GATED_LATCH u5 (.CP(cp1), .D(d), .S(s0), .Q(q_gated));
  GATED_LATCH_ARRAY u6 (.CP0(cp1), .CP1(cp0), .D(d), .S0(s0), .S1(s1), .Q(q_gated2));
  LATCH_LOW u7 (.EN(en), .D(d), .Q(q_low));
endmodule
