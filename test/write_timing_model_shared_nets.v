module shared_nets_blk(clk, a, d, y0, y1, t0, t1, t2, q, z);
  input clk, a, d;
  output y0, y1, t0, t1, t2, q, z;
  BUF u1(.A(a), .Y(y0));
  assign y1 = y0;
  assign t0 = 1'b0;
  assign t1 = 1'b0;
  assign t2 = 1'b1;
  AND2 u2(.A(d), .B(1'b0), .Y(z));
  DFF r1(.CK(clk), .D(d), .Q(q));
endmodule
