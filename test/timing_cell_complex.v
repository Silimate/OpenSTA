module timing_cell_complex (in, clk, out);
  
  input in, clk;
  output out;
  wire w1, w2, w3, w4;

  timing_cell_dff tt1(.in(in), .clk(clk), .out(w1));

  timing_cell_comb tt2(.in(w1), .out(w2));

  timing_cell_dff tt3(.in(w2), .clk(clk), .out(w3));

  BUF_X1 u1 (.A(w3), .Z(w4));
  BUF_X1 u2 (.A(w4), .Z(out));

endmodule
