import { useEffect, useState } from 'react';
import { Text, View, StyleSheet, Image, FlatList, Button } from 'react-native';
import { rnNitroCache } from 'react-native-nitro-cache';

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
          rnNitroCache.clear();
        }}
      />
      <Button
        title="Entries"
        onPress={() => {
          rnNitroCache.getEntries().then((entries) => {
            console.log('entries', entries);
          });
        }}
      />
      <Button
        title="Stats"
        onPress={() => {
          rnNitroCache.getStats().then((stats) => {
            console.log('stats', stats);
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
          contentContainerStyle={{
            rowGap: 10,
          }}
        />
      )}
    </View>
  );
}

const ImageComponent = ({ url }: { url: string }) => {
  const [result, setResult] = useState<string | null>(null);
  const get = async (u: string) => {
    const res = await rnNitroCache.getOrFetch(u);
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
    <View style={{ width: '100%', flexDirection: 'row' }}>
      <Image source={{ uri: `file://${result}` }} style={styles.image} />
      <Button
        title="Info"
        onPress={() =>
          rnNitroCache.get(url).then((entry) => {
            console.log('entry', entry);
          })
        }
      />
      <Button
        title="Has"
        onPress={() => {
          console.log('has', rnNitroCache.has(url));
        }}
      />
      <Button
        title="Buffer"
        onPress={() =>
          rnNitroCache.getBuffer(url).then((buffer) => {
            console.log('buffer', buffer?.byteLength);
          })
        }
      />
      <Button title="Delete" onPress={() => rnNitroCache.remove(url)} />
    </View>
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
