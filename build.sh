#!/bin/bash
if make; then
	echo Build succeeded.
	make run;
else
	echo Build failed
fi
