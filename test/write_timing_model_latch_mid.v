module latch_mid(clk, a, b, y, z);
  input clk, a, b;
  output y, z;
  wire ma, y0, w0;
  DFF m0(.CK(clk), .D(a), .Q(ma));
  latch_blk k0(.clk(clk), .a(ma), .b(b), .y(y0), .z(z), .w(w0));
  DFF m1(.CK(clk), .D(w0), .Q(y));
endmodule
