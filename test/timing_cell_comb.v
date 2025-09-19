module timing_cell_comb (in, clk, out);
  input in, clk;
  output out;

  BUFx2_ASAP7_75t_R u2 (.A(in), .Y(out));

endmodule
