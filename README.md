riffdate
========

This program tries to extract create time info from a RIFF file.

	% ./riffdate DSCN0345.AVI
	DateTimeOriginal: 2013:06:11 13:30:25
	CreateDate: 2013:06:11 13:30:25
	% ./riffdate SDIM0149.AVI
	IDIT: THU FEB 13 11:03:15 2011
	% 

This works on AVI files produced by the following cameras at least.

	SIGMA DP2
	SIGMA DP2 Merrill
	Nikon COOLPIX S30

Currently this looks at RIFF IDIT and Nikon vendor-specific tags.
