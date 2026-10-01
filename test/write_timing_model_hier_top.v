module top(ck1, ck2, in0, rst, out0, out1, out2);
  input ck1, ck2, in0, rst;
  output out0, out1, out2;
  wire c1, q0, y0, z0, y1;
  CLKBUF tc1(.A(ck1), .Y(c1));
  DFF t0(.CK(ck1), .D(in0), .Q(q0));
  // ports swapped on i1 so clka/clkb see different top clocks
  mid i0(.clka(c1), .clkb(ck2), .a(q0), .rn(rst), .y(y0), .z(z0));
  mid i1(.clka(ck2), .clkb(ck1), .a(y0), .rn(rst), .y(out0), .z(out1));
  // same top clock on both ports
  blk i2(.clka(ck1), .clkb(ck1), .a(z0), .rn(rst), .y(out2), .z());
endmodule
