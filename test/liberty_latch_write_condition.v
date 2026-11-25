module top(input cp0, cp1, d, s0, s1,
           output q_table_01, q_table_10, q_latch_01, q_latch_10, q_default);
  TABLE_01 table_01 (.CP0(cp0), .CP1(cp1), .D(d), .S0(s0), .S1(s1), .Q(q_table_01));
  TABLE_10 table_10 (.CP0(cp0), .CP1(cp1), .D(d), .S0(s0), .S1(s1), .Q(q_table_10));
  LATCH_01 latch_01 (.CP0(cp0), .CP1(cp1), .D(d), .S0(s0), .S1(s1), .Q(q_latch_01));
  LATCH_10 latch_10 (.CP0(cp0), .CP1(cp1), .D(d), .S0(s0), .S1(s1), .Q(q_latch_10));
  TABLE_DEFAULT default_arc (.CP0(cp0), .CP1(cp1), .D(d), .S0(s0), .S1(s1), .Q(q_default));
endmodule
