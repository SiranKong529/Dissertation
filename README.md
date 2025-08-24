1. Generating Chord Audio Files
	•	To create the .WAV sound files, run all the Python scripts in the ChordGeneration folder.
	•	Copy all the generated .WAV files onto a micro SD card, which should then be inserted into the M5Stack Core2 modules.

⸻

2. Uploading Band Member Code
	•	In the band_members folder, there are five files named after the band members.
	•	Each file must be uploaded to the corresponding M5Stack Core2 (e.g., guitar.ino → Guitar robot’s M5Stack).

⸻

3. Uploading Improvisation and Voting Logic
	•	The improvisation_test file must be uploaded to the 3pi+ robot of the sender (drummer).
	•	There are two voting mechanism files for the receivers:
	•	receivers_test → Default method
	•	receivers_Markov → Markov-based method
	•	Upload one of these files to each of the four 3pi+ receiver robots.
	•	⚠️ Remember to set the correct MEMBER_ID inside each receiver file before uploading.

⸻

4. Processing Results
	•	The Result_process folder contains Python scripts for evaluation and analysis.
	•	Run all .py files in this folder to process the experimental results.
