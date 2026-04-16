import { useEffect } from 'react';
import { Text, View, StyleSheet, Image } from 'react-native';
import { NitroCacheHybridObject } from 'react-native-nitro-cache';

// import { fetch } from 'react-native-nitro-fetch'

// const result = multiply(3, 7);

export default function App() {
  // const [result, setResult] = useState(null);
  const get = async () => {
    const res = await NitroCacheHybridObject.getOrFetch(
      // 'https://join.ostrom.de/images/tariff-plang-header.back.png'
      'https://picsum.photos/200/300'
    );
    console.log('result from js ', res);
    // if (res) {
    //   setResult(res.url);
    // }
    // const exists = NitroCacheHybridObject.has(
    //   'https://join.ostrom.de/images/tariff-plang-header.back.png'
    // );
    // console.log('exists ', exists);
    // const stats = await NitroCacheHybridObject.getStats();
    // console.log('stats ', stats);
    // const entries = await NitroCacheHybridObject.getEntries();
    // console.log('entries ', entries);
    // const buffer = await NitroCacheHybridObject.getBuffer(
    //   'https://join.ostrom.de/images/tariff-plang-header.back.png'
    // );
    // how to log the buffer
    // console.log('buffer ', buffer?.byteLength);
    // const cleared = await NitroCacheHybridObject.clear();
    // console.log('clear result ', cleared);
  };
  // const get = async () => {
  //   const result = await fetch('https://join.ostrom.de/images/tariff-plang-header.back.png');
  //   const json = await result.json();
  //   console.log("result from js 2 ", json);
  // };
  useEffect(() => {
    get();
  }, []);

  return (
    <View style={styles.container}>
      <Text>Result</Text>
      <Image
        source={{
          uri: 'https://join.ostrom.de/images/tariff-plang-header.back.png',
        }}
        onError={(e) => {
          console.log(e.nativeEvent.error);
        }}
        style={styles.image}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
  image: {
    width: 100,
    height: 100,
  },
});
