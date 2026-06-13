

cards = [ ("delay","delay"),
          ("euclidean","euclideanrhythms"),
          ("bitcrush","bitcrusher"),
          ("attenuvert","attenuverter"),
          ("clockdiv","clockdivider"),
          ("slopesplus","slopesplus"),
          ("maxrect","maxslashrectify"),
          ("chords","chords"),
          ("windowcomp","windowcomparator"),
          ("karplusstrong","karplusstrong"),
          ("cross","crossswitch"),
          ("cvmix","cvmixer"),
          ("vco","vco"),
          ("chorus","chorus"),
          ("wavefolder","wavefolder"),
          ("lpg","lowpassgate"),
          ("sandh","sampleandhold"),
          ("quantiser","quantiser"),
          ("bernoulli","bernoulligate"),
          ("supersaw","supersaw"),
          ("vca","vca"),
          ("slowlfo","slowlfo"),
          ("turing185","turing185"),
          ("glitch","glitch") ]
cards = sorted(cards)

print(f'unsigned numUtilities = {len(cards)}')
print(f'const unsigned char * utility_names[{len(cards)}] = '+'{',end='')

for c in cards:
    print(f'name_{c[1]}, ',end='')
print('};')

for i1,c1 in enumerate(cards):
    for i2,c2 in enumerate(cards):
        print(f'if (utilityIndex[0] == {i1} && utilityIndex[1] == {i2}) cc = new UtilityPair<{c1[0]}, {c2[0]}>;')
