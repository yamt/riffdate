riffdate
========

This program tries to extract create time info from a RIFF file.

	% ./riffdate DP2M2622.AVI 
	IDIT-exiftime: 2013:06:27 21:24:22
	% ./riffdate DSCN1060.AVI 
	ntcg-DateTimeOriginal: 2013:06:27 21:27:28
	ntcg-CreateDate: 2013:06:27 21:27:28
	% 

This works on AVI files produced by the following cameras at least.

	SIGMA DP2
	SIGMA DP2 Merrill
	Nikon COOLPIX S30

Currently this looks at RIFF IDIT and Nikon vendor-specific tags.
