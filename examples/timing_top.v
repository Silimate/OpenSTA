module timing_top (in, clk, out);
  input in, clk;
  output out;
  wire w1, w2, w3;

  timing_cell tc1 (.in(in), .clk(clk), .out(w1));

  BUF_X1 u1 (.A(w1), .Z(w2));

  timing_cell tc2 (.in(w2), .clk(clk), .out(w3));

  BUF_X1 u2 (.A(w3), .Z(out));

endmodule