module io_delays_blk(clk, a, b, d, y, q);
  input clk, a, b, d;
  output y, q;
  AND2 u1(.A(a), .B(b), .Y(y));
  DFF r1(.CK(clk), .D(d), .Q(q));
endmodule
