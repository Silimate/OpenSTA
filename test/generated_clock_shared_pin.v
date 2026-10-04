module generated_clock_shared_pin (clk, q1, q2);
  input clk;
  output q1, q2;
  CLK_SHARE u0 (.CLK_IN(clk), .Q1(q1), .Q2(q2));
endmodule
