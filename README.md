# sineLab
sineLab, an additive synthesizer with 12843 sine wave oscillators. Available for free on Github as AU and VST. sineLab is made with JUCE and Claude.

sineLab is short for sine wave laboratory. There are 88 MIDI keys, ranging from A0 to C8, all with a specific number of sine wave oscillators allocated to them. The formula for determining the number of sine wave oscillators for a specific key between A0 and C8 is 20000/fundamental, or 20000 Hz, divided by the fundamental frequency of each MIDI key between A0 and C8, assuming equal temperament. A0 has 727 sine wave oscillators, while C8 has only 4. 

sineLab has no FX, filters, modulation, apreggiation, sequencing, automation, MIDI CCs, or audio files. There are only 12843 sine wave oscillators across 88 keys, and ways to make fast organized changes, but nothing else. THere are only sine wave oscillators, and every oscillator is identical, only differing in default frequency. Every oscillator can be toggled, altered in amplitude, cents deviation, starting phase, stereo panned, and every oscillator has it's own ADSR. 

sineLab has 2 methods of editing; global mode, or changes across keys, and key mode, or focus on the harmonics of a single key. The point of sineLab is to have many oscillators, but be able to make fast and organized changes across many.

sineLab by default opens in global mode. Thus, when NO key is selected at the bottom, sineLab assumes you want to make changes across the keyboard. sineLab has 9 tabs: TOGGLE, AMP, PHASE, PAN, ATTACK, DECAY, SUSTAIN, RELEASE. 



TOGGLE
Toggle tab shows the currently active harmonics. The upper left box represents the amount of active harmonics for A0, and the right represents the amount of harmonics for C8. The horizontal axis of the global toggle tab represents MIDI keys from A0 to C8. The vertical axis represents the number of harmonics per key. The bottom row is the 1st harmonic for every key, the 2nd is the second, and so on. The graph is simply a way to visually see which harmonics are currently ON. 

The 1, 3, and e buttons simply represent linear, cubic, or exponential interpolation. THe steepness of e ranges from +-50. 

Primes toggle toggles on or off the prime-numbered harmonics. Thus, 2, 3, 5, 7, 11, 13, 17, 23, 29, etc. are toggled. 

Evens toggle toggles on or off the even-numbered harmonics. 2, 4, 6, 8, etc. 

1st Harm toggles on or off the 1st harmonic, or fundamental of every key. 

The blank checkbox represents toggling ALL harmonics on or off. 



AMP
The top left black box represents the MIDI number, thus 1 represents A0, 88 represents C8. There are also blue numbers on every key at the bottom representing the number of that key. If this number is anything greater than 1, all keys with a number less than it will get an identical Duty Cycle value. The black box immediately to the right of this smaller black box represents the value of the duty cycle for the key denoted by the number in the smaller left box, and all keys to the left of it. For the 2 blue boxes to the right, the behavior is identical, except all the keys GREATER than the number in the left blue box get identical values. 

The 1, 2, 3, ||, e buttons are buttons that allow one to interpolate the duty cycle between the chosen endpoints. 

1/[n*n] button sets the amplitudes of every key to be 1 divided by it's harmonic number, but squared. Thus the first harmonic gets 1 amplitude, the second gets 1/4, third gets 1/9, etc. 

1/n 1/[n*n] button sets the amplitudes of every key to be 1 divided by it's harmonic number. Thus the first harmonic gets 1 amplitude, the second gets 1/2, third gets 1/2, etc.

TAPER ACT is a toggle in the global AMP tab that multiplies (k - n + 1)/k to every CURRENTLY ACTIVE harmonic of every key. Where k is the active harmonic count for that key, n is the harmonic number. 

TAPER CT is a toggle in the global AMP tab that multiplies (k - n + 1)/k to every harmonic of every key. Where k is the total harmonic count for that key, n is the harmonic number. It smooths out the Gibbs phenomena, and does "darken" the tone of these classical waveforms.

NORM toggle normalizes the volume OF AN ENTIRE KEY, based on the root mean square of the sum of the amplitudes of every harmonic for that key, then the result multiplied times 3/4. Thus any change made to the toggle or amplitude tab affects the normalization.

The box under norm allows one to apply a single amplitude value to every harmonic.

The Duty Cycle tab is the same as tab "I". Tab "II" represents the amplitude slider for every key, and thus if one desires a different balance between keys, one can interpolate between keys in tab II. The upper 2 buttons and text boxes work the same; choose 2 keys as endpoints, and interpolate between them if desired. 

The graph has the horizontal axis to be the MIDI number, and the vertical axis represents the duty cycle from 0 to 50% assuming one wants to interpolate the duty cycle of a pulse wave between the endpoints.




TUNE
STRETCH's boxes behave similarly to AMP's; Left text box represents A0. Right, C8. What happens in stretch is one cent deviation value is added. Meaning if I type in 50 in the left box, +50 cents will be applied to every harmonic in A0. The graph only shows and allows plus or minus 50 cents for stretch. 

The 5 interpolation buttons interpolate between the endpoints in cent deviation.

II of TUNE represents inharmonicity. The formula for inharmonicity used in sineLab is n(f_0)(sqrt[1 + Bn^2]), where B is the inharmonicity coefficient. B ranges between +-0.0015. By default, there is no inharmonicity, thus leaving this at 0 will result in a tuning most synths already assume. 

The bottom right box in TUNE represents the cent deviation applied to every harmonic.

The graph shows the amount of stretch applied to every key. The horizontal axis represents A0 to C8's tuning deviation, and the vertical axis represents +-50 cents.



PHASE
Starting phase ranges from 1 to 360 in sineLab. Starting phase for every MIDI event is whatever the values are in the global PHASE tab. The left box represents the phase value applied to every harmonic of A0. The right is for every harmonic of C8.  

The 5 function buttons allow one to interpolate the starting phase values for every key's harmonics between A0 and C8. 

EVENS means the phase value applied to all even numbered harmonics.

RAND ranges from 0.01 to 1.00, or 0 to 100%. Typing in a value, and pressing enter, or clicking away, randomizes one time, the starting phase of every harmonic of every key by THAT percentage
set in the RAND box. This only happens once, and thus an additional randomizization requires one to press enter on that value again; the randomization does not accumulate or stack.

Bottom right text box sets the same phase value to every harmonic of every key. 

Phase in sineLab is intended to be stored and interpolated as integers. 

The horizontal axis represents the keys between A0 and C8. Vertical azis ranges from 1 to 360.



PAN
The top box of PAN represents how wide the buttons below will spread harmonics. One can enter a value between plus or minus 1 to determine how wide they want the buttons below to spread. 

2L means skip the fundamental, pan the 2nd and 3rd harmonics LEFT, the 4th and 5th right, 6th and 7th left, and so on until there are no more harmonics. 

LR skips the fundamental, and pans the next ACTIVE harmonic left, then the next active right, then the next left, and so on. 

OLER pans all odds left, evens right. ELOR pans evens left, odds right. How wide they are panned depends on the value in the upper pan box.



For the rest of the tabs, the horizontal axis represents the value of the keys between A0 and C8. The bottom right text box will always allow one to apply one value to every harmonic.

ATTACK
The top boxes determine the attack speed for all of A0's and C8's harmonics respectively. The 5 buttons allow for interpolation between. RAND is identical in behavior to PHASE's RAND; the RAND box here represents a percentage of randomization deviating from the current attack values. Thus typing 0.01 and pressing enter means all harmonics are randomized once by 1% from where they currently are.



DECAY
The two boxes represent the decay values applied to all harmonics of A0 and C8. The 5 interpolation buttons allow for interpolation. Bottom right box allows one to apply one decay value to every harmonic. 

II tab uses the fundamental and the highest active harmonic as interpolation points. One can interpolate between them the decay times of harmonics between those endpoints. The top left box of decay II sets the percentage of harmonics from the fundamental. Thus typing in .5 means the first 50% of harmonics will get the same decay value as the fundamental, and the highest harmonic of that 50% is the new endpoint for interpolation. 



SUSTAIN
The bottom right box sets one sustain value to every harmonic. Ranges from 0.0000 to 1.0000.



RELEASE
Top left box represent's the release value for all of A0's harmonics, right for C8. Same buttons. RAND box works identically for release as it does for the rest of the tabs. 



State is saved, but sineLab features no preset browser. The default preset features of most major DAWs is assumed to be used. 
