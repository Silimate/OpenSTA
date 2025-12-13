module generated_clock (
  input wire SYS_CLK_IN,
  output wire slow_clk_int,
  output wire fast_clk_int,
  output wire slow_clk_out_div4
);

  second_hierarchy u_second_hierarchy (
    .slow_clk_int(SYS_CLK_IN),
    .slow_clk_out(slow_clk_int),
    .fast_clk_out(fast_clk_int),
    .slow_clk_out_div4(slow_clk_out_div4)
  );

endmodule

module second_hierarchy (
  input wire slow_clk_int,
  output wire slow_clk_out,
  output wire fast_clk_out,
  output wire slow_clk_out_div4
);

  CLK_GEN my_divider (
    .CLK_IN(slow_clk_int),
    .CLK_OUT_DIV2(slow_clk_out),
    .CLK_OUT_MUL2(fast_clk_out)
  );

  NESTED_CLKGEN divide_by_4 (
    .CLK_IN(slow_clk_out),
    .CLK_OUT_DIV2(slow_clk_out_div4)
  );

endmodule