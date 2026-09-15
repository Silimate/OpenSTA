module top (in1, in2, clk, out1, out2, bus);
  input in1, in2, clk;
  output out1, out2;
  output [1:0] bus;
  wire r1q, u1y;

  DFFHQx4_ASAP7_75t_R r1 (.D(in1), .CLK(clk), .Q(r1q));
  AND2x2_ASAP7_75t_R u1 (.A(r1q), .B(in2), .Y(u1y));
  BUFx2_ASAP7_75t_R u2 (.A(u1y), .Y(out1));
  INVx2_ASAP7_75t_R u3 (.A(u1y), .Y(out2));
  // Unconnected output pin.
  BUFx2_ASAP7_75t_R u4 (.A(in2), .Y());
  // Bus members, to check bit blasted ports resolve like scalar ones.
  BUFx2_ASAP7_75t_R u5 (.A(r1q), .Y(bus[0]));
  INVx2_ASAP7_75t_R u6 (.A(in2), .Y(bus[1]));
endmodule // top
