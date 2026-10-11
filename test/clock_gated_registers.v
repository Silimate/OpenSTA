module top (clk, en, d1, d2, q1, q2);
  input clk, en, d1, d2;
  output q1, q2;
  wire gclk;
  CLKGATE_X1 icg (.CK(clk), .E(en), .GCK(gclk));
  DFF_X1 r1 (.D(d1), .CK(gclk), .Q(q1));
  DFF_X1 r2 (.D(d2), .CK(clk), .Q(q2));
endmodule
