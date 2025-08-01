module timing_cell_comb (in, clk, out);
  input in, clk;
  output out;

  BUF_X1 u2 (.A(in), .Z(out));

endmodule
