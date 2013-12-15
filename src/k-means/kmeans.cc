#include "kmeans.hh"

using namespace std;

/* For testing purposes, let's generate a few SIFT keypoints here. */
void processSIFTPoints(){
	/* Load an image to compute the SIFT features on */
	cBitmap bitmap;
	char filename[32];

	strcpy( filename, "../../media/apple.bmp" );			//From HW2
	bitmap.loadBitmap( filename );
	int number_pixels = bitmap.getWidth()*bitmap.getHeight();

	unsigned char * buffer = new unsigned char[number_pixels];
	bitmap.getGreyscaleBitmap( buffer, number_pixels );		//SIFT accepts greyscale only

	/* Convert to correct format (float/vl_sift_pix) */
	vl_sift_pix * im = new vl_sift_pix[number_pixels];

	for( int i = 0; i < bitmap.getWidth()*bitmap.getHeight(); i++ )
		im[i] = static_cast<vl_sift_pix>( buffer[i]/256.0f );		//From testing: Both scaling to [0,1] or leaving at [0,255] are acceptable. vl_sift_pix = float.
	
	delete[] buffer;

	#ifdef DEBUG
	unsigned char * debug_im = new unsigned char[number_pixels*sizeof(Pixel)];
	bitmap.getBitmap( debug_im, number_pixels*sizeof(Pixel) );
	#endif

	/* Create SIFT Filter object */
	VlSiftFilt * s = vl_sift_new( bitmap.getWidth(), bitmap.getHeight(), 3, 3, 0 );
	vl_sift_process_first_octave( s, im );

	int iteration = 0;
	do{	
		/* Detect and retrieve keypoints in this octave */
		vl_sift_detect( s );
		const VlSiftKeypoint * keypoint = vl_sift_get_keypoints( s );
		int num_keys = vl_sift_get_nkeypoints( s );

		#ifdef DEBUG
		cout << num_keys << " keypoints were detected!" << endl;
		#endif

		for( int i = 0; i < num_keys; i++ ){
			#ifdef DEBUG
			cout << "Keypoint at x=" << ceil(keypoint[i].x) << ", y=" << ceil(keypoint[i].y) << ":" << endl;
			int coord = ceil(keypoint[i].x)+(ceil(keypoint[i].y)*bitmap.getWidth());				//For visualization of keypoints, you can set an appropriate color here
			debug_im[coord*4] = 255; debug_im[coord*4+1] = 255;
			debug_im[coord*4+2] = 0; debug_im[coord*4+3] = 0;
			#endif

			double angles[4];
			int num_angles = vl_sift_calc_keypoint_orientations( s, angles, &(keypoint[i]) );
			cout << "\t" << num_angles << " orientations found!" << endl;

			for( int j = 0; j < num_angles; j++ ){
				/* Get 128 bin histogram */
				vl_sift_pix * output = new vl_sift_pix[128];
				memset( output, 0, 128*sizeof( vl_sift_pix ) );

				vl_sift_calc_keypoint_descriptor( s, output, &(keypoint[i]), angles[j] );
				//Save feature here!
				delete[] output;
			}
		}

		cout << "Iteration " << ++iteration << endl;
	}while( vl_sift_process_next_octave( s ) != VL_ERR_EOF );

	#ifdef DEBUG
	glutViewer( debug_im, bitmap.getWidth(), bitmap.getHeight(), 4, 0, NULL, 600, 600 );	//Displays a bitmap image. See viewer.cc/hh
	delete[] debug_im;
	#endif

	vl_sift_delete( s );
}


int main( int argc, char ** argv ){
	//The centroids can be used for the creation of visual words
	//After finding features in an image, compute distances to all k centroids
	//and choose best fit
	vector<SIFTFeature> dataset;

	#ifdef CLUSTER
	/* Testing */
	KMeansClustering c(CLUSTER_K);
	c.loadDataset( dataset );
	c.lloyds( dataset );
	
	VisualWord test;
	c.convertToVisualWord( test, dataset[0] );
	#endif


	/* Testing area */
	processSIFTPoints();
	return 0;
}


KMeansClustering::KMeansClustering( int k ){
	this->k = k;
	centroids = new SIFTFeature[k];
}

KMeansClustering::~KMeansClustering(){
	delete[] centroids;
	dataset.clear();
}

/* Here we will load the SIFT features into memory */
void KMeansClustering::loadDataset( vector<SIFTFeature> & db ){
	loadRandomDataset( db );
}


//For this test, we'll load some random data
//Clustering random data? Good idea...lol
void KMeansClustering::loadRandomDataset( vector<SIFTFeature> & db ){
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

/*  Convert the SIFTFeature given in @feature to a visual word in @result */
/* Can this be sped up with hashing? */
void KMeansClustering::convertToVisualWord( VisualWord & result, SIFTFeature & feature ){
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
void KMeansClustering::lloyds( vector<SIFTFeature> & db ){
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
	while( doIteration( db, assignment ) ){ loop++; }	

	cout << "Required iterations were: " << loop << endl;

	//At this point, assignment holds our choosen centroids/words
	//centroids can be used to determine the appropriate word for a keypoint


	delete[] assignment;
	delete[] c;
}

void KMeansClustering::getUniqueUniformRandom( int * data, int index, int limit ){
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
bool KMeansClustering::doIteration( vector<SIFTFeature> & db, int * assignment ){
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
void KMeansClustering::addSIFT( SIFTFeature & a, SIFTFeature b ){
	for( int i = 0; i < 128; i++ )
		a.orientations[0].histogram[i] += b.orientations[0].histogram[i];
	
}

//Divide SIFT vector a by the scalar b
void KMeansClustering::divideSIFT( SIFTFeature & a, double b ){
	for( int i = 0; i < 128; i++ ){
		a.orientations[0].histogram[i] /= b;
	}
}

//Choosing block-distance for this test
double KMeansClustering::sift_distance( SIFTFeature a, SIFTFeature b ){
	double d = 0;
	for( int i = 0; i < 128; i++ )
		d += std::abs(a.orientations[0].histogram[i] - b.orientations[0].histogram[i]);
	return d;
}
