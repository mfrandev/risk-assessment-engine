/ Compute shocks from `market` table and store them in `shocks`.

errorExit:{[msg]
    -1 enlist "ERROR: ", msg;
    system "exit 1";
  };

ensureMarketExists:{
    if[not `market in key `.;
      errorExit "No market data found in market. Load market data first."];
    if[98h<>type market;
      errorExit "market must be a table."];
  };

calcColumn:{[tbl; col]
    data:tbl[col];
    if[2>count data;
      errorExit "Market data requires at least two rows to compute shocks."];
    previous:data[til (count data) - 1];
    if[0>=any previous;
      errorExit "Encountered non-positive base price in column ", string col];
    current:1_ data;
    :(current % previous) - 1.0e0;
  };


ensureMarketExists[];
priceCols:(cols market) except `date;
if[0=count priceCols;
  errorExit "Market table has no price columns."];
shocksCols:priceCols!(calcColumn[market;] each priceCols);
dates:1_ market[`date];
shocksDict:enlist[`date]!enlist dates;
shocksDict:shocksDict , shocksCols;
shocks:flip shocksDict;
`shocks set shocks;
info["Computed shocks table with ", (string count shocks), " rows."];
shocks;

