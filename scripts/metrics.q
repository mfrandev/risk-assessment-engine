/ R: T×N matrix (list of T rows; each row a float list of length N)
/ mu: length-N float vector (column means)
/ returns: N×N sample covariance matrix
computeSampleCovariance:{[R; mu]
        T: count R;
        if[T<=1; : (count mu)#(count mu)#0f];
        X: flip ((flip R) - mu);                     / center columns
        (flip X) mmu X % T-1           / (X^T X)/(T-1)
    };

/ shocks has columns: `date plus tickers; we drop `date and build R, mu
getSampleCovarianceFromShocks:{
    columns: 1_ cols shocks;
    R: shocks[;columns];        / T×N matrix (rows are days)
    mu: avg each flip R;                   / column means
    computeSampleCovariance[R; mu]
    };

computeSampleMean: {[tab]
    :avg each tab each 1_ cols tab;
    };

getSampleMeanFromShocks: {
    :computeSampleMean shocks;
    };