.PHONY:
	clean

CXXFLAGS=-O3 -Wall -Wextra

spectro: spectro.cpp
	$(CXX) $(CXXFLAGS) -o spectro spectro.cpp -lsndfile
