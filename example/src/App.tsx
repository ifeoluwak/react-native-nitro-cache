import { useEffect, useState } from 'react';
import { Text, View, StyleSheet, Image, FlatList, Button } from 'react-native';
import { NitroCacheHybridObject } from 'react-native-nitro-cache';

// import { fetch } from 'react-native-nitro-fetch'

// const result = multiply(3, 7);

// 'https://picsum.photos/250/250',
// '/Devices/16AD3ADE-BDFC-4BBC-BE35-E07FBA45F087/data/Containers/Data/Application/CE42B521-AB31-4F64-83C5-BD69E19D4107/Library/Caches/nitro-cache/4bd5b3c1ab9d19afd32121a08d6a8fe1d0c6aec6c471f46a7aa8dd1c3af27f72.jpeg'

// 'Devices/16AD3ADE-BDFC-4BBC-BE35-E07FBA45F087/data/Containers/Data/Application/B69DF0B0-EB08-4268-8DD3-058599FAEF1A/Library/Caches/nitro-cache/4bd5b3c1ab9d19afd32121a08d6a8fe1d0c6aec6c471f46a7aa8dd1c3af27f72.jpeg'

// const imgUrls = [
//   'https://join.ostrom.de/images/tariff-plang-header.back.png',
//   'https://picsum.photos/200/200',
//   'https://picsum.photos/300/300',
//   'https://picsum.photos/400/400',
//   'https://picsum.photos/250/250',
//   'https://picsum.photos/150/150',
//   'https://picsum.photos/280/280',
//   'https://picsum.photos/320/320',
// ];
const imgUrls2 = [
  'https://picsum.photos/200/200?blur=2',
  'https://picsum.photos/300/300?blur=2',
  'https://picsum.photos/400/400?blur=2',
  'https://picsum.photos/250/250?blur=2',
  'https://picsum.photos/150/150?blur=2',
  'https://picsum.photos/280/280?blur=2',
  'https://picsum.photos/320/320?blur=2',
  'https://picsum.photos/420/420?blur=2',
  'https://picsum.photos/520/520?blur=2',
  'https://picsum.photos/620/620?blur=2',
  'https://picsum.photos/720/720?blur=2',
  'https://picsum.photos/820/820?blur=2',
  'https://picsum.photos/920/920?blur=2',
];

export default function App() {
  const [showImages, setShowImages] = useState(false);
  return (
    <View style={styles.container}>
      <Text>Result</Text>
      <Button
        title="Clear all"
        onPress={() => {
          NitroCacheHybridObject.clear();
        }}
      />
      <Button
        title="Entries"
        onPress={() => {
          NitroCacheHybridObject.getEntries().then((entries) => {
            console.log('entries', entries);
          });
        }}
      />
      <Button
        title="Show Images"
        onPress={() => {
          setShowImages(true);
        }}
      />
      {showImages && (
        <FlatList
          data={imgUrls2}
          keyExtractor={(item) => item}
          renderItem={({ item }) => <ImageComponent url={item} />}
        />
      )}
    </View>
  );
}

const ImageComponent = ({ url }: { url: string }) => {
  const [result, setResult] = useState<string | null>(null);
  const get = async (u: string) => {
    const res = await NitroCacheHybridObject.getOrFetch(u);
    if (res) {
      setResult(res.url);
    }
  };
  useEffect(() => {
    if (url) {
      get(url);
    }
  }, [url]);

  return (
    <>
      <Image
        source={{ uri: `file://${result}` }}
        style={styles.image}
        onError={() => {
          // console.log(e.nativeEvent.error);
        }}
      />
      <Button
        title="Delete"
        onPress={() => NitroCacheHybridObject.remove(url)}
      />
    </>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
    paddingTop: 100,
  },
  image: {
    width: 90,
    height: 90,
  },
});
