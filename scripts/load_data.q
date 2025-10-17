// Helper logger
info:{[msg] -1 enlist "INFO (", (string .z.p), "): ", msg};

// =================================Import Market Data Dynamically=================================

// Static filepath
marketPath: hsym `$"data/market/market_scenario1.csv";

inferHeader:{[f]
    hdr: first read0 f;            / raw header line
    info["Market CSV header: ", hdr];
    xs: "," vs raze (hdr except "\r");   / split, strip CR
    if[0=count xs; '"empty header"];
    firstColName: `$lower first xs;
    if[firstColName <> `date;  '"first column must be date"];
    syms: `$ 1_ xs;          / tickers as symbols
    if[(count syms)<>count distinct syms; '"duplicate tickers in header"]
    id2sym: syms;                  / id = index
    sym2id: syms!til count syms;   / map symbol -> id
    (`date; syms; id2sym; sym2id)
  };

readMarketData:{[f]
  (date;tickers;id2ticker;ticker2id): inferHeader[f];
  :("D",(count tickers)#"F";enlist csv) 0: f;
  };

market: readMarketData marketPath;

// =================================Import Portfolio Data=================================

// Static filepath
portfolioPath: hsym `$"data/portfolio/portfolio_scenario1.csv"

readPortfolioData: {[f]
  :("IIBFFFIFFFF";enlist csv) 0: f;
  };

portfolio: readPortfolioData portfolioPath;
