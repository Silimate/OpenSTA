module timing_cell_complex (in, clk, out);
  
  input in, clk;
  output out;
  wire w1, w2, w3;

  timing_cell_dff tt1(.in(in), .clk(clk), .out(w1));

  timing_cell_comb tt2(.in(w1), .out(w2));

  BUF_X1 u1 (.A(w2), .Z(w3));

  timing_cell_dff tt3(.in(w3), .clk(clk), .out(out));

endmodule
