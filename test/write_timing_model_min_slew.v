module min_slew_blk(a, b, y);
  input a, b;
  output y;
  AND2S u1(.A(a), .B(b), .Y(y));
endmodule

module flat_top(a, b, y);
  input a, b;
  output y;
  wire n;
  min_slew_blk i0(.a(a), .b(b), .y(n));
  BUF u2(.A(n), .Y(y));
endmodule

module model_top(a, b, y);
  input a, b;
  output y;
  wire n;
  min_slew_model i0(.a(a), .b(b), .y(n));
  BUF u2(.A(n), .Y(y));
endmodule
