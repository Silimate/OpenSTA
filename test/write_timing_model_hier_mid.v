// Mid-level block with its own registers around two blk instances.
module mid(clka, clkb, a, rn, y, z);
  input clka, clkb, a, rn;
  output y, z;
  wire ma, y0, z0, y1, mz;
  DFF m0(.CK(clka), .D(a), .Q(ma));
  blk k0(.clka(clka), .clkb(clkb), .a(ma), .rn(rn), .y(y0), .z(z0));
  blk k1(.clka(clkb), .clkb(clka), .a(y0), .rn(rn), .y(y1), .z(z));
  DFF m1(.CK(clkb), .D(y1), .Q(mz));
  BUF mb(.A(mz), .Y(y));
endmodule
