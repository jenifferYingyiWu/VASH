#include "kmeans.hh"

using namespace std;

vector<SIFTFeature> dataset;

int main( int argc, char ** argv ){
	//The centroids can be used for the creation of visual words
	//After finding features in an image, compute distances to all k centroids
	//and choose best fit
	SIFTFeature * centroids = new SIFTFeature[CLUSTER_K];

	loadDataset( dataset );
	lloyds( dataset, centroids, CLUSTER_K );

	VlSiftFilt * s = vl_sift_new( 256, 256, 5, 3, 0 );
	s = s;
	delete[] centroids;
	return 0;
}

/* Here we will load the SIFT features into memory */
void loadDataset( vector<SIFTFeature> & db ){
	loadRandomDataset( db );
}


//For this test, we'll load some random data
//Clustering random data? Good idea...lol
void loadRandomDataset( vector<SIFTFeature> & db ){
	std::mt19937 generator( 0 );	//Or another seed. But this is just a test, so...
	std::normal_distribution<double> distribution( 0.0, 1.0 );

	int dim = 128;		//Test: Dimensionality of each keypoint
	int elem = 1000;	//Test: Number of keypoints

	for( int i = 0; i < elem; i++ ){
		SIFTFeature s;
		memset( &s, 0, sizeof( SIFTFeature ) );

		for( int j = 0; j < dim; j++ ){
  			s.orientations[0].histogram[j] = distribution(generator);
		}

		db.push_back( s );
	}
}

/*  Convert the SIFTFeature given in @feature to a visual word in @result
	using the precomputed @centroids */
/* Can this be sped up with hashing? */
void convertToVisualWord( VisualWord & result, SIFTFeature & feature, SIFTFeature * centroids, int k ){
	double min_distance = HUGE_VAL;
	int min_index = -1;
	for( int i = 0; i < k; i++ ){
		double d = sift_distance( feature, centroids[i] );

		if( d < min_distance ){
			d = min_distance;
			min_index = i;
		}
	}

	result.id = min_index;
}


//Clustering algorithm
void lloyds( vector<SIFTFeature> & db, SIFTFeature * centroids, int k ){
	int * c = new int[k];

	int * assignment = new int[db.size()];

	memset( assignment, 0, db.size()*sizeof( int ) );

	//Should we try to speed up initialisation by hashing?
	//Possibly: Hash all features, choose k buckets as centroids
	//But will it be faster?	
	for( int i = 0; i < k; i++ )
		getUniqueUniformRandom( c, i, db.size() );	//Depending on k and db.size(), you may also assign w/o checking for uniqueness
	
	for( int i = 0; i < k; i++ )
		centroids[i] = db[c[i]];

	int loop = 0;
	//k-means is proven to have an upper bound in the number of iterations
	//If this goes to infinite loop, there is a bug!
	while( doIteration( db, assignment, centroids, k ) ){ loop++; }	

	cout << "Required iterations were: " << loop << endl;

	//At this point, assignment holds our choosen centroids/words
	//centroids can be used to determine the appropriate word for a keypoint


	delete[] assignment;
	delete[] c;
}

void getUniqueUniformRandom( int * data, int index, int limit ){
	bool fresh = true;
	int seed = (index+1)*time( NULL );

	std::default_random_engine generator( seed );
	std::uniform_int_distribution<int> distribution( 0, limit );

	while( true ){
		int r = distribution(generator);
	
		for( int j = 0; j < index; j++ )
			if( data[j] == r ){
				fresh = false; 
				break;
			}

		if( fresh ){
			data[index] = r;
			break;
		}
	}
}

//One iteration of k-means (lloyds)
bool doIteration( vector<SIFTFeature> & db, int * assignment, SIFTFeature * centroids, int k ){
	int * old_assignment = new int[db.size()];
	memcpy( old_assignment, assignment, db.size()*sizeof( int ) );

	vector<SIFTFeature> * inverted_list = new vector<SIFTFeature>[k];		//This makes updating the centroids somewhat easier

	for( vector<SIFTFeature>::iterator it = db.begin(); it != db.end(); ++it ){
		
		double min_dist = HUGE_VAL;
		int best_index = 0;

		//Compute distances to all current centroids
		for( int i = 0; i < k; i++ ){
			double d = sift_distance( *it, centroids[i] );
			if( d < min_dist ){
				min_dist = d;
				best_index = i;
			}
		}

		assignment[it-db.begin()] = best_index;
		inverted_list[best_index].push_back( *it );
	}
	
	/* Update centroids */
	//For each of the k clusters ...
	for( int i = 0; i < k; i++ ){
		memset( &centroids[i], 0, sizeof( SIFTFeature ) );	//Set the mean to zero

		//Sum up vectors
		for( vector<SIFTFeature>::iterator it = inverted_list[i].begin(); it != inverted_list[i].end(); ++it )
			addSIFT( centroids[i], *it );

		//Scale them by number of vectors (-> arithmetic mean)
		divideSIFT( centroids[i], inverted_list[i].size() );
	}

	/* Check if this iteration changed anything */
	bool changes = false;

	for( unsigned int i = 0; i < db.size(); i++ ){
		if( assignment[i] != old_assignment[i] ){
			changes = true;
			break;
		}
	}


	for( int i = 0; i < k; i++ )
		inverted_list[i].clear();

	delete[] inverted_list;
	delete[] old_assignment;
	return changes;
}

//Add the SIFT feature vector of b onto a
void addSIFT( SIFTFeature & a, SIFTFeature b ){
	for( int i = 0; i < 128; i++ )
		a.orientations[0].histogram[i] += b.orientations[0].histogram[i];
	
}

//Divide SIFT vector a by the scalar b
void divideSIFT( SIFTFeature & a, double b ){
	for( int i = 0; i < 128; i++ ){
		a.orientations[0].histogram[i] /= b;
	}
}

//Choosing block-distance for this test
double sift_distance( SIFTFeature a, SIFTFeature b ){
	double d = 0;
	for( int i = 0; i < 128; i++ )
		d += std::abs(a.orientations[0].histogram[i] - b.orientations[0].histogram[i]);
	return d;
}