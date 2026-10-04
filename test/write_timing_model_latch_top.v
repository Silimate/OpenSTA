module latch_top(ck, in0, in1, out0, out1, out2, out3);
  input ck, in0, in1;
  output out0, out1, out2, out3;
  wire c1, q0, y0, t1q;
  CLKBUF tc(.A(ck), .Y(c1));
  DFF t0(.CK(ck), .D(in0), .Q(q0));
  latch_blk i0(.clk(c1), .a(q0), .b(in1), .y(y0), .z(out0), .w(out1));
  latch_mid i1(.clk(ck), .a(y0), .b(q0), .y(out2), .z(out3));
endmodule
