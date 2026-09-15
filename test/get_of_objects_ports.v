module top (in1, in2, clk, out1, out2);
  input in1, in2, clk;
  output out1, out2;
  wire r1q, u1y;

  DFFHQx4_ASAP7_75t_R r1 (.D(in1), .CLK(clk), .Q(r1q));
  AND2x2_ASAP7_75t_R u1 (.A(r1q), .B(in2), .Y(u1y));
  BUFx2_ASAP7_75t_R u2 (.A(u1y), .Y(out1));
  INVx2_ASAP7_75t_R u3 (.A(u1y), .Y(out2));
  // Unconnected output pin.
  BUFx2_ASAP7_75t_R u4 (.A(in2), .Y());
endmodule // top
